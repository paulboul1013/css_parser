#include "css_token.h"
#include <stdlib.h>
#include <string.h>

css_token *css_token_create(css_token_type type) {
    css_token *t = calloc(1, sizeof(css_token));
    if (!t) return NULL;
    t->type = type;
    return t;
}

void css_token_free(css_token *token) {
    if (!token) return;
    free(token->value);
    free(token->unit);
    free(token);
}

const char *css_token_type_name(css_token_type type) {
    switch (type) {
        case CSS_TOKEN_IDENT:         return "ident";
        case CSS_TOKEN_FUNCTION:      return "function";
        case CSS_TOKEN_AT_KEYWORD:    return "at-keyword";
        case CSS_TOKEN_HASH:          return "hash";
        case CSS_TOKEN_STRING:        return "string";
        case CSS_TOKEN_BAD_STRING:    return "bad-string";
        case CSS_TOKEN_URL:           return "url";
        case CSS_TOKEN_BAD_URL:       return "bad-url";
        case CSS_TOKEN_DELIM:         return "delim";
        case CSS_TOKEN_NUMBER:        return "number";
        case CSS_TOKEN_PERCENTAGE:    return "percentage";
        case CSS_TOKEN_DIMENSION:     return "dimension";
        case CSS_TOKEN_WHITESPACE:    return "whitespace";
        case CSS_TOKEN_CDO:           return "CDO";
        case CSS_TOKEN_CDC:           return "CDC";
        case CSS_TOKEN_COLON:         return "colon";
        case CSS_TOKEN_SEMICOLON:     return "semicolon";
        case CSS_TOKEN_COMMA:         return "comma";
        case CSS_TOKEN_OPEN_SQUARE:   return "[";
        case CSS_TOKEN_CLOSE_SQUARE:  return "]";
        case CSS_TOKEN_OPEN_PAREN:    return "(";
        case CSS_TOKEN_CLOSE_PAREN:   return ")";
        case CSS_TOKEN_OPEN_CURLY:    return "{";
        case CSS_TOKEN_CLOSE_CURLY:   return "}";
        case CSS_TOKEN_EOF:           return "EOF";
        default:                      return "unknown";
    }
}
