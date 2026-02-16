#ifndef CSS_TOKENIZER_H
#define CSS_TOKENIZER_H

#include "css_token.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define CSS_EOF_CODEPOINT 0xFFFFFFFF

typedef struct {
    char *input;           /* Preprocessed copy (owned, must free) */
    size_t length;         /* Length of preprocessed input */
    size_t pos;            /* Current byte offset */

    uint32_t current;      /* Current code point */
    uint32_t peek1;        /* Lookahead +1 */
    uint32_t peek2;        /* Lookahead +2 */
    uint32_t peek3;        /* Lookahead +3 */

    size_t line;           /* Current line (1-based) */
    size_t column;         /* Current column (1-based) */

    bool reconsume;        /* Reconsume flag */
} css_tokenizer;

css_tokenizer *css_tokenizer_create(const char *input, size_t length);
css_token     *css_tokenizer_next(css_tokenizer *t);
void           css_tokenizer_free(css_tokenizer *t);

#endif /* CSS_TOKENIZER_H */
