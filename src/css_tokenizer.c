#include "css_tokenizer.h"
#include <stdlib.h>
#include <string.h>

/* ---------- UTF-8 decode ---------- */

/*
 * Decode a single UTF-8 code point from s (max len bytes available).
 * Sets *bytes_read to number of bytes consumed.
 * Returns code point, or CSS_EOF_CODEPOINT if at end.
 */
static uint32_t decode_utf8(const char *s, size_t len, size_t *bytes_read)
{
    if (len == 0) {
        *bytes_read = 0;
        return CSS_EOF_CODEPOINT;
    }

    unsigned char b0 = (unsigned char)s[0];

    /* 1-byte (ASCII): 0xxxxxxx */
    if (b0 < 0x80) {
        *bytes_read = 1;
        return b0;
    }

    /* 2-byte: 110xxxxx 10xxxxxx */
    if ((b0 & 0xE0) == 0xC0) {
        if (len < 2 || ((unsigned char)s[1] & 0xC0) != 0x80) {
            *bytes_read = 1;
            return 0xFFFD;
        }
        uint32_t cp = ((uint32_t)(b0 & 0x1F) << 6)
                     | ((uint32_t)((unsigned char)s[1]) & 0x3F);
        /* Overlong check: must be >= 0x80 */
        if (cp < 0x80) {
            *bytes_read = 1;
            return 0xFFFD;
        }
        *bytes_read = 2;
        return cp;
    }

    /* 3-byte: 1110xxxx 10xxxxxx 10xxxxxx */
    if ((b0 & 0xF0) == 0xE0) {
        if (len < 3
            || ((unsigned char)s[1] & 0xC0) != 0x80
            || ((unsigned char)s[2] & 0xC0) != 0x80) {
            *bytes_read = 1;
            return 0xFFFD;
        }
        uint32_t cp = ((uint32_t)(b0 & 0x0F) << 12)
                     | ((uint32_t)((unsigned char)s[1] & 0x3F) << 6)
                     | ((uint32_t)((unsigned char)s[2]) & 0x3F);
        /* Overlong check: must be >= 0x800 */
        if (cp < 0x800) {
            *bytes_read = 1;
            return 0xFFFD;
        }
        *bytes_read = 3;
        return cp;
    }

    /* 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if ((b0 & 0xF8) == 0xF0) {
        if (len < 4
            || ((unsigned char)s[1] & 0xC0) != 0x80
            || ((unsigned char)s[2] & 0xC0) != 0x80
            || ((unsigned char)s[3] & 0xC0) != 0x80) {
            *bytes_read = 1;
            return 0xFFFD;
        }
        uint32_t cp = ((uint32_t)(b0 & 0x07) << 18)
                     | ((uint32_t)((unsigned char)s[1] & 0x3F) << 12)
                     | ((uint32_t)((unsigned char)s[2] & 0x3F) << 6)
                     | ((uint32_t)((unsigned char)s[3]) & 0x3F);
        /* Overlong check: must be >= 0x10000 and <= 0x10FFFF */
        if (cp < 0x10000 || cp > 0x10FFFF) {
            *bytes_read = 1;
            return 0xFFFD;
        }
        *bytes_read = 4;
        return cp;
    }

    /* Invalid leading byte */
    *bytes_read = 1;
    return 0xFFFD;
}

/* ---------- Lookahead helpers ---------- */

/*
 * Decode code point at given byte position.
 * Sets *bytes to number of bytes consumed.
 */
static uint32_t peek_at(css_tokenizer *t, size_t byte_pos, size_t *bytes)
{
    if (byte_pos >= t->length) {
        *bytes = 0;
        return CSS_EOF_CODEPOINT;
    }
    return decode_utf8(t->input + byte_pos, t->length - byte_pos, bytes);
}

/*
 * Fill all 4 lookahead slots from current position.
 * Called once during initialization.
 */
static void fill_lookahead(css_tokenizer *t)
{
    size_t p = t->pos;
    size_t bytes;

    t->current = peek_at(t, p, &bytes);
    p += bytes;

    t->peek1 = peek_at(t, p, &bytes);
    p += bytes;

    t->peek2 = peek_at(t, p, &bytes);
    p += bytes;

    t->peek3 = peek_at(t, p, &bytes);
    /* Don't advance p further; we only need 4 slots */
}

/*
 * Advance one code point: shift peek pipeline, decode next.
 * Updates line/column tracking.
 */
static void consume_codepoint(css_tokenizer *t)
{
    /* Update line/column based on the code point we are consuming */
    if (t->current == '\n') {
        t->line++;
        t->column = 1;
    } else if (t->current != CSS_EOF_CODEPOINT) {
        t->column++;
    }

    /* Skip past current code point's bytes in the input */
    size_t bytes;
    (void)peek_at(t, t->pos, &bytes);
    t->pos += bytes;

    /* Shift the pipeline */
    t->current = t->peek1;
    t->peek1   = t->peek2;
    t->peek2   = t->peek3;

    /* Decode the next code point for peek3 */
    /* We need to find where peek3 starts: pos + bytes for current + peek1 + peek2 */
    /* But since we already shifted, we need to compute the byte position after
       the new peek2. We'll scan forward from t->pos past 3 code points. */
    size_t scan = t->pos;
    size_t b;
    /* skip current (already at scan = t->pos, which is the new current) */
    (void)peek_at(t, scan, &b); scan += b;  /* past new current -> new peek1 */
    (void)peek_at(t, scan, &b); scan += b;  /* past new peek1 -> new peek2 */
    (void)peek_at(t, scan, &b); scan += b;  /* past new peek2 -> new peek3 */

    t->peek3 = peek_at(t, scan, &b);
}

/* ---------- Preprocessing (CSS Syntax §3.3) ---------- */

/*
 * Preprocess the raw input:
 *  - CRLF (0x0D 0x0A) -> LF (0x0A)
 *  - CR (0x0D alone)   -> LF (0x0A)
 *  - FF (0x0C)         -> LF (0x0A)
 *  - NULL (0x00)       -> U+FFFD (0xEF 0xBF 0xBD in UTF-8)
 *
 * Returns a malloc'd buffer; sets *out_len to the length.
 */
static char *preprocess(const char *input, size_t length, size_t *out_len)
{
    /* Worst case: every byte is NULL -> 3 bytes each */
    char *buf = malloc(length * 3 + 1);
    if (!buf) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)input[i];

        if (c == 0x0D) {
            /* CR: check for CRLF */
            if (i + 1 < length && (unsigned char)input[i + 1] == 0x0A) {
                i++;  /* skip the LF in CRLF pair */
            }
            buf[j++] = '\n';
        } else if (c == 0x0C) {
            /* FF -> LF */
            buf[j++] = '\n';
        } else if (c == 0x00) {
            /* NULL -> U+FFFD (3 bytes in UTF-8) */
            buf[j++] = (char)0xEF;
            buf[j++] = (char)0xBF;
            buf[j++] = (char)0xBD;
        } else {
            buf[j++] = (char)c;
        }
    }
    buf[j] = '\0';
    *out_len = j;
    return buf;
}

/* ---------- Public API ---------- */

css_tokenizer *css_tokenizer_create(const char *input, size_t length)
{
    css_tokenizer *t = calloc(1, sizeof(css_tokenizer));
    if (!t) return NULL;

    size_t pp_len = 0;
    t->input = preprocess(input, length, &pp_len);
    if (!t->input) {
        free(t);
        return NULL;
    }
    t->length = pp_len;

    t->pos       = 0;
    t->line      = 1;
    t->column    = 1;
    t->reconsume = false;

    /* Fill the 4-slot lookahead pipeline */
    fill_lookahead(t);

    return t;
}

css_token *css_tokenizer_next(css_tokenizer *t)
{
    /* TODO: full token dispatch */
    (void)consume_codepoint;  /* suppress unused warning until dispatch is implemented */
    css_token *tok = css_token_create(CSS_TOKEN_EOF);
    if (tok) {
        tok->line   = t->line;
        tok->column = t->column;
    }
    return tok;
}

void css_tokenizer_free(css_tokenizer *t)
{
    if (!t) return;
    free(t->input);
    free(t);
}
