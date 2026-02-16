#ifndef CSS_TOKEN_H
#define CSS_TOKEN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    CSS_TOKEN_IDENT,
    CSS_TOKEN_FUNCTION,
    CSS_TOKEN_AT_KEYWORD,
    CSS_TOKEN_HASH,
    CSS_TOKEN_STRING,
    CSS_TOKEN_BAD_STRING,
    CSS_TOKEN_URL,
    CSS_TOKEN_BAD_URL,
    CSS_TOKEN_DELIM,
    CSS_TOKEN_NUMBER,
    CSS_TOKEN_PERCENTAGE,
    CSS_TOKEN_DIMENSION,
    CSS_TOKEN_WHITESPACE,
    CSS_TOKEN_CDO,
    CSS_TOKEN_CDC,
    CSS_TOKEN_COLON,
    CSS_TOKEN_SEMICOLON,
    CSS_TOKEN_COMMA,
    CSS_TOKEN_OPEN_SQUARE,
    CSS_TOKEN_CLOSE_SQUARE,
    CSS_TOKEN_OPEN_PAREN,
    CSS_TOKEN_CLOSE_PAREN,
    CSS_TOKEN_OPEN_CURLY,
    CSS_TOKEN_CLOSE_CURLY,
    CSS_TOKEN_EOF
} css_token_type;

typedef enum {
    CSS_NUM_INTEGER,
    CSS_NUM_NUMBER
} css_number_type;

typedef enum {
    CSS_HASH_UNRESTRICTED,
    CSS_HASH_ID
} css_hash_type;

typedef struct {
    css_token_type type;

    /* String value (IDENT, FUNCTION, AT_KEYWORD, HASH, STRING, URL) */
    char *value;

    /* Numeric value (NUMBER, PERCENTAGE, DIMENSION) */
    double numeric_value;
    css_number_type number_type;

    /* Unit (DIMENSION only, e.g. "px", "em") */
    char *unit;

    /* Hash type flag (HASH only) */
    css_hash_type hash_type;

    /* Delim single codepoint (DELIM only) */
    uint32_t delim_codepoint;

    /* Position info (debugging) */
    size_t line;
    size_t column;
} css_token;

css_token *css_token_create(css_token_type type);
void css_token_free(css_token *token);
const char *css_token_type_name(css_token_type type);

#endif /* CSS_TOKEN_H */
