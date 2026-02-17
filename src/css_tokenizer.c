#define _POSIX_C_SOURCE 200809L
#include "css_tokenizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>

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

/* ---------- Check helpers (§4.3.7–§4.3.12) ---------- */

/* §4.3.8: Two code points are a valid escape? */
static bool valid_escape(uint32_t c1, uint32_t c2)
{
    return c1 == '\\' && c2 != '\n';
}

/* §4.3.10: Three code points would start a number? */
static bool starts_number(uint32_t c1, uint32_t c2, uint32_t c3)
{
    if (c1 == '+' || c1 == '-') {
        if (is_digit(c2)) return true;
        if (c2 == '.' && is_digit(c3)) return true;
        return false;
    }
    if (c1 == '.') {
        return is_digit(c2);
    }
    return is_digit(c1);
}

/* §4.3.9: Three code points would start an ident sequence? */
static bool starts_ident_sequence(uint32_t c1, uint32_t c2, uint32_t c3)
{
    if (c1 == '-') {
        return is_ident_start(c2) || c2 == '-' || valid_escape(c2, c3);
    }
    if (is_ident_start(c1)) return true;
    if (c1 == '\\') return valid_escape(c1, c2);
    return false;
}

/* Encode a code point as UTF-8 into buf. Returns bytes written. */
static size_t encode_utf8(uint32_t cp, char *buf, size_t buf_size)
{
    if (cp < 0x80 && buf_size >= 1) {
        buf[0] = (char)cp;
        return 1;
    } else if (cp < 0x800 && buf_size >= 2) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000 && buf_size >= 3) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF && buf_size >= 4) {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

/* §4.3.7: Consume an escaped code point */
static uint32_t consume_escaped_codepoint(css_tokenizer *t)
{
    if (t->current == CSS_EOF_CODEPOINT) {
        css_parse_error(t, "EOF in escape");
        return 0xFFFD;
    }
    if (is_hex_digit(t->current)) {
        uint32_t value = 0;
        int count = 0;
        while (is_hex_digit(t->current) && count < 6) {
            uint32_t d;
            if (t->current >= '0' && t->current <= '9') d = t->current - '0';
            else if (t->current >= 'A' && t->current <= 'F') d = t->current - 'A' + 10;
            else d = t->current - 'a' + 10;
            value = value * 16 + d;
            consume_codepoint(t);
            count++;
        }
        /* Optional trailing whitespace */
        if (is_whitespace(t->current)) consume_codepoint(t);
        /* Validate */
        if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
            return 0xFFFD;
        return value;
    }
    uint32_t cp = t->current;
    consume_codepoint(t);
    return cp;
}

/* §4.3.12: Consume a number. Sets *out_type to integer or number. */
static double consume_number(css_tokenizer *t, css_number_type *out_type)
{
    *out_type = CSS_NUM_INTEGER;

    char repr[128];
    size_t ri = 0;

    /* Optional sign */
    if (t->current == '+' || t->current == '-') {
        repr[ri++] = (char)t->current;
        consume_codepoint(t);
    }

    /* Integer part: digits */
    while (is_digit(t->current)) {
        repr[ri++] = (char)t->current;
        consume_codepoint(t);
    }

    /* Decimal part: '.' followed by digits */
    if (t->current == '.' && is_digit(t->peek1)) {
        *out_type = CSS_NUM_NUMBER;
        repr[ri++] = '.';
        consume_codepoint(t); /* consume '.' */
        while (is_digit(t->current)) {
            repr[ri++] = (char)t->current;
            consume_codepoint(t);
        }
    }

    /* Exponent part: 'e' or 'E', optional sign, digits */
    if ((t->current == 'e' || t->current == 'E') &&
        (is_digit(t->peek1) ||
         ((t->peek1 == '+' || t->peek1 == '-') && is_digit(t->peek2)))) {
        *out_type = CSS_NUM_NUMBER;
        repr[ri++] = (char)t->current;
        consume_codepoint(t); /* consume 'e'/'E' */
        if (t->current == '+' || t->current == '-') {
            repr[ri++] = (char)t->current;
            consume_codepoint(t);
        }
        while (is_digit(t->current)) {
            repr[ri++] = (char)t->current;
            consume_codepoint(t);
        }
    }

    repr[ri] = '\0';
    return strtod(repr, NULL);
}

/* §4.3.11: Consume an ident sequence, return as strdup'd string */
static char *consume_ident_sequence(css_tokenizer *t)
{
    char buf[256];
    size_t len = 0;

    while (len < sizeof(buf) - 4) { /* leave room for UTF-8 */
        if (is_ident_char(t->current)) {
            buf[len++] = (char)t->current;
            consume_codepoint(t);
        } else if (valid_escape(t->current, t->peek1)) {
            consume_codepoint(t); /* consume backslash */
            uint32_t cp = consume_escaped_codepoint(t);
            len += encode_utf8(cp, buf + len, sizeof(buf) - len);
        } else {
            break;
        }
    }
    buf[len] = '\0';
    return strdup(buf);
}

/* §4.3.14: Consume the remnants of a bad url */
static void consume_bad_url_remnants(css_tokenizer *t)
{
    for (;;) {
        if (t->current == ')' || t->current == CSS_EOF_CODEPOINT) {
            if (t->current == ')') consume_codepoint(t);
            return;
        }
        if (valid_escape(t->current, t->peek1)) {
            consume_codepoint(t); /* consume '\' */
            consume_escaped_codepoint(t);
        } else {
            consume_codepoint(t);
        }
    }
}

/* §4.3.5: Consume a string token */
static css_token *consume_string_token(css_tokenizer *t, uint32_t ending)
{
    size_t tok_line = t->line, tok_col = t->column;
    consume_codepoint(t); /* consume the opening quote */

    char buf[4096];
    size_t len = 0;

    for (;;) {
        if (t->current == CSS_EOF_CODEPOINT) {
            css_parse_error(t, "unterminated string");
            break; /* return what we have as string-token */
        }
        if (t->current == ending) {
            consume_codepoint(t); /* consume closing quote */
            break;
        }
        if (t->current == '\n') {
            /* Newline in string → parse error + bad-string-token */
            css_parse_error(t, "newline in string");
            /* Don't consume the newline */
            css_token *tok = css_token_create(CSS_TOKEN_BAD_STRING);
            buf[len] = '\0';
            tok->value = strdup(buf);
            tok->line = tok_line;
            tok->column = tok_col;
            return tok;
        }
        if (t->current == '\\') {
            if (t->peek1 == CSS_EOF_CODEPOINT) {
                consume_codepoint(t); /* consume backslash, EOF next */
                continue;
            }
            if (t->peek1 == '\n') {
                /* Escaped newline → continuation, consume both */
                consume_codepoint(t); /* consume '\' */
                consume_codepoint(t); /* consume '\n' */
                continue;
            }
            /* Valid escape */
            consume_codepoint(t); /* consume '\' */
            uint32_t cp = consume_escaped_codepoint(t);
            if (len < sizeof(buf) - 4) {
                len += encode_utf8(cp, buf + len, sizeof(buf) - len);
            }
            continue;
        }
        /* Normal character */
        if (len < sizeof(buf) - 4) {
            len += encode_utf8(t->current, buf + len, sizeof(buf) - len);
        }
        consume_codepoint(t);
    }

    buf[len] = '\0';
    css_token *tok = css_token_create(CSS_TOKEN_STRING);
    tok->value = strdup(buf);
    tok->line = tok_line;
    tok->column = tok_col;
    return tok;
}

/* §4.3.6: Consume a url token */
static css_token *consume_url_token(css_tokenizer *t, size_t tok_line, size_t tok_col)
{
    /* Whitespace after url( has already been consumed by consume_ident_like_token */
    char buf[4096];
    size_t len = 0;

    for (;;) {
        if (t->current == CSS_EOF_CODEPOINT) {
            css_parse_error(t, "unterminated URL");
            break;
        }
        if (t->current == ')') {
            consume_codepoint(t);
            break;
        }
        if (is_whitespace(t->current)) {
            /* Skip whitespace, then expect ')' */
            while (is_whitespace(t->current)) consume_codepoint(t);
            if (t->current == ')') {
                consume_codepoint(t);
                break;
            }
            if (t->current == CSS_EOF_CODEPOINT) {
                css_parse_error(t, "unterminated URL");
                break;
            }
            /* Bad URL */
            css_parse_error(t, "unexpected character in URL");
            consume_bad_url_remnants(t);
            css_token *tok = css_token_create(CSS_TOKEN_BAD_URL);
            tok->line = tok_line;
            tok->column = tok_col;
            return tok;
        }
        /* Bad characters in URL: ", ', (, non-printable */
        if (t->current == '"' || t->current == '\'' || t->current == '(' || is_non_printable(t->current)) {
            css_parse_error(t, "bad character in URL");
            consume_bad_url_remnants(t);
            css_token *tok = css_token_create(CSS_TOKEN_BAD_URL);
            tok->line = tok_line;
            tok->column = tok_col;
            return tok;
        }
        if (t->current == '\\') {
            if (valid_escape(t->current, t->peek1)) {
                consume_codepoint(t); /* consume '\' */
                uint32_t cp = consume_escaped_codepoint(t);
                if (len < sizeof(buf) - 4) {
                    len += encode_utf8(cp, buf + len, sizeof(buf) - len);
                }
                continue;
            }
            /* Invalid escape in URL */
            css_parse_error(t, "invalid escape in URL");
            consume_bad_url_remnants(t);
            css_token *tok = css_token_create(CSS_TOKEN_BAD_URL);
            tok->line = tok_line;
            tok->column = tok_col;
            return tok;
        }
        /* Normal character */
        if (len < sizeof(buf) - 4) {
            len += encode_utf8(t->current, buf + len, sizeof(buf) - len);
        }
        consume_codepoint(t);
    }

    buf[len] = '\0';
    css_token *tok = css_token_create(CSS_TOKEN_URL);
    tok->value = strdup(buf);
    tok->line = tok_line;
    tok->column = tok_col;
    return tok;
}

/* §4.3.4: Consume an ident-like token */
static css_token *consume_ident_like_token(css_tokenizer *t)
{
    size_t tok_line = t->line, tok_col = t->column;
    char *name = consume_ident_sequence(t);

    /* Check for url( — case-insensitive */
    if (strcasecmp(name, "url") == 0 && t->current == '(') {
        consume_codepoint(t); /* consume '(' */
        /* Skip whitespace to see if quoted or unquoted URL */
        while (is_whitespace(t->current)) {
            consume_codepoint(t);
        }
        if (t->current == '\'' || t->current == '"') {
            /* url("...") or url('...') → function token */
            css_token *tok = css_token_create(CSS_TOKEN_FUNCTION);
            tok->value = name;
            tok->line = tok_line;
            tok->column = tok_col;
            return tok;
        }
        /* Unquoted URL */
        free(name);
        return consume_url_token(t, tok_line, tok_col);
    }

    /* name followed by '(' → function token */
    if (t->current == '(') {
        consume_codepoint(t); /* consume '(' */
        css_token *tok = css_token_create(CSS_TOKEN_FUNCTION);
        tok->value = name;
        tok->line = tok_line;
        tok->column = tok_col;
        return tok;
    }

    /* Otherwise → ident token */
    css_token *tok = css_token_create(CSS_TOKEN_IDENT);
    tok->value = name;
    tok->line = tok_line;
    tok->column = tok_col;
    return tok;
}

/* §4.3.3: Consume a numeric token */
static css_token *consume_numeric_token(css_tokenizer *t)
{
    size_t tok_line = t->line, tok_col = t->column;

    css_number_type num_type;
    double value = consume_number(t, &num_type);

    /* Check for dimension: starts_ident_sequence? */
    if (starts_ident_sequence(t->current, t->peek1, t->peek2)) {
        css_token *tok = css_token_create(CSS_TOKEN_DIMENSION);
        tok->numeric_value = value;
        tok->number_type = num_type;
        tok->unit = consume_ident_sequence(t);
        tok->line = tok_line;
        tok->column = tok_col;
        return tok;
    }

    /* Check for percentage */
    if (t->current == '%') {
        consume_codepoint(t);
        css_token *tok = css_token_create(CSS_TOKEN_PERCENTAGE);
        tok->numeric_value = value;
        tok->number_type = num_type;
        tok->line = tok_line;
        tok->column = tok_col;
        return tok;
    }

    /* Plain number */
    css_token *tok = css_token_create(CSS_TOKEN_NUMBER);
    tok->numeric_value = value;
    tok->number_type = num_type;
    tok->line = tok_line;
    tok->column = tok_col;
    return tok;
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

    /* '"' → string token */
    if (c == '"') {
        return consume_string_token(t, '"');
    }

    /* '\'' → string token */
    if (c == '\'') {
        return consume_string_token(t, '\'');
    }

    /* Digit -> numeric token */
    if (is_digit(c)) {
        return consume_numeric_token(t);
    }

    /* '#' → hash token (§4.3.1) */
    if (c == '#') {
        if (is_ident_char(t->peek1) || valid_escape(t->peek1, t->peek2)) {
            consume_codepoint(t); /* consume '#' */
            css_token *tok = css_token_create(CSS_TOKEN_HASH);
            if (starts_ident_sequence(t->current, t->peek1, t->peek2)) {
                tok->hash_type = CSS_HASH_ID;
            } else {
                tok->hash_type = CSS_HASH_UNRESTRICTED;
            }
            tok->value = consume_ident_sequence(t);
            tok->line = tok_line;
            tok->column = tok_col;
            return tok;
        }
        /* else fall through to delim */
    }

    /* '+' → might start number */
    if (c == '+') {
        if (starts_number(c, t->peek1, t->peek2)) {
            return consume_numeric_token(t);
        }
        /* else fall through to delim */
    }

    /* '-' → number / CDC / ident / delim */
    if (c == '-') {
        if (starts_number(c, t->peek1, t->peek2)) {
            return consume_numeric_token(t);
        }
        /* CDC: --> */
        if (t->peek1 == '-' && t->peek2 == '>') {
            consume_codepoint(t); /* '-' */
            consume_codepoint(t); /* '-' */
            consume_codepoint(t); /* '>' */
            return make_token(CSS_TOKEN_CDC, tok_line, tok_col);
        }
        /* ident starting with '-' */
        if (starts_ident_sequence(c, t->peek1, t->peek2)) {
            return consume_ident_like_token(t);
        }
        /* else fall through to delim */
    }

    /* '.' → might start number */
    if (c == '.') {
        if (starts_number(c, t->peek1, t->peek2)) {
            return consume_numeric_token(t);
        }
        /* else fall through to delim */
    }

    /* '<' → CDO (<!--) */
    if (c == '<') {
        if (t->peek1 == '!' && t->peek2 == '-' && t->peek3 == '-') {
            consume_codepoint(t); /* '<' */
            consume_codepoint(t); /* '!' */
            consume_codepoint(t); /* '-' */
            consume_codepoint(t); /* '-' */
            return make_token(CSS_TOKEN_CDO, tok_line, tok_col);
        }
        /* else fall through to delim */
    }

    /* '@' → at-keyword token */
    if (c == '@') {
        if (starts_ident_sequence(t->peek1, t->peek2, t->peek3)) {
            consume_codepoint(t); /* consume '@' */
            css_token *tok = css_token_create(CSS_TOKEN_AT_KEYWORD);
            tok->value = consume_ident_sequence(t);
            tok->line = tok_line;
            tok->column = tok_col;
            return tok;
        }
        /* else fall through to delim */
    }

    /* '\' (backslash) → valid escape → ident-like token */
    if (c == '\\') {
        if (valid_escape(c, t->peek1)) {
            return consume_ident_like_token(t);
        }
        css_parse_error(t, "invalid escape");
        /* fall through to delim */
    }

    /* ident-start → ident-like token */
    if (is_ident_start(c)) {
        return consume_ident_like_token(t);
    }

    /* Everything else: delim token */
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
