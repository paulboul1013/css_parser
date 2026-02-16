#include "css_tokenizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- Code point classification helpers ---------- */

static bool is_whitespace(uint32_t c)
{
    return c == '\n' || c == '\t' || c == ' ';
}

static bool is_digit(uint32_t c)
{
    return c >= '0' && c <= '9';
}

static bool is_hex_digit(uint32_t c)
{
    return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static bool is_letter(uint32_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool is_non_ascii(uint32_t c)
{
    return c >= 0x80 && c != CSS_EOF_CODEPOINT;
}

static bool is_ident_start(uint32_t c)
{
    return is_letter(c) || is_non_ascii(c) || c == '_';
}

static bool is_ident_char(uint32_t c)
{
    return is_ident_start(c) || is_digit(c) || c == '-';
}

static bool is_non_printable(uint32_t c)
{
    return (c <= 0x08) || c == 0x0B || (c >= 0x0E && c <= 0x1F) || c == 0x7F;
}

/* Suppress unused warnings for helpers used in future tasks */
static inline void unused_helpers_(void)
{
    (void)is_hex_digit;
    (void)is_ident_start;
    (void)is_ident_char;
    (void)is_non_printable;
}

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

/* ---------- Parse error helper ---------- */

static void css_parse_error(css_tokenizer *t, const char *msg)
{
    if (getenv("CSSPARSER_PARSE_ERRORS")) {
        fprintf(stderr, "CSS parse error at %zu:%zu: %s\n",
                t->line, t->column, msg);
    }
}

/* ---------- Comment consumption (CSS Syntax §4.3.2) ---------- */

static void consume_comments(css_tokenizer *t)
{
    while (t->current == '/' && t->peek1 == '*') {
        consume_codepoint(t); /* consume '/' */
        consume_codepoint(t); /* consume '*' */
        for (;;) {
            if (t->current == CSS_EOF_CODEPOINT) {
                css_parse_error(t, "unterminated comment");
                return;
            }
            if (t->current == '*' && t->peek1 == '/') {
                consume_codepoint(t); /* consume '*' */
                consume_codepoint(t); /* consume '/' */
                break;
            }
            consume_codepoint(t);
        }
    }
}

/* ---------- Token creation helper ---------- */

static css_token *make_token(css_token_type type, size_t line, size_t col)
{
    css_token *tok = css_token_create(type);
    if (tok) {
        tok->line   = line;
        tok->column = col;
    }
    return tok;
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
    /* Suppress unused helpers (used in future tasks) */
    (void)unused_helpers_;

    /* Consume comments first (CSS Syntax §4.3.2) */
    consume_comments(t);

    uint32_t c = t->current;
    size_t tok_line = t->line;
    size_t tok_col  = t->column;

    /* EOF */
    if (c == CSS_EOF_CODEPOINT) {
        return make_token(CSS_TOKEN_EOF, tok_line, tok_col);
    }

    /* Whitespace token: consume consecutive whitespace */
    if (is_whitespace(c)) {
        consume_codepoint(t);
        while (is_whitespace(t->current)) {
            consume_codepoint(t);
        }
        return make_token(CSS_TOKEN_WHITESPACE, tok_line, tok_col);
    }

    /* Single-character tokens */
    if (c == '(') { consume_codepoint(t); return make_token(CSS_TOKEN_OPEN_PAREN,   tok_line, tok_col); }
    if (c == ')') { consume_codepoint(t); return make_token(CSS_TOKEN_CLOSE_PAREN,  tok_line, tok_col); }
    if (c == '[') { consume_codepoint(t); return make_token(CSS_TOKEN_OPEN_SQUARE,  tok_line, tok_col); }
    if (c == ']') { consume_codepoint(t); return make_token(CSS_TOKEN_CLOSE_SQUARE, tok_line, tok_col); }
    if (c == '{') { consume_codepoint(t); return make_token(CSS_TOKEN_OPEN_CURLY,   tok_line, tok_col); }
    if (c == '}') { consume_codepoint(t); return make_token(CSS_TOKEN_CLOSE_CURLY,  tok_line, tok_col); }
    if (c == ':') { consume_codepoint(t); return make_token(CSS_TOKEN_COLON,        tok_line, tok_col); }
    if (c == ';') { consume_codepoint(t); return make_token(CSS_TOKEN_SEMICOLON,    tok_line, tok_col); }
    if (c == ',') { consume_codepoint(t); return make_token(CSS_TOKEN_COMMA,        tok_line, tok_col); }

    /* Everything else: delim token (ident, number, etc. handled in future tasks) */
    consume_codepoint(t);
    css_token *tok = css_token_create(CSS_TOKEN_DELIM);
    if (tok) {
        tok->delim_codepoint = c;
        tok->line   = tok_line;
        tok->column = tok_col;
    }
    return tok;
}

void css_tokenizer_free(css_tokenizer *t)
{
    if (!t) return;
    free(t->input);
    free(t);
}
