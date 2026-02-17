#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "css_tokenizer.h"

int main(int argc, char *argv[])
{
    bool token_mode = false;
    const char *filename = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            token_mode = true;
        } else if (!filename) {
            filename = argv[i];
        }
    }

    if (!filename) {
        fprintf(stderr, "Usage: %s [--tokens] <file.css>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror(filename);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fprintf(stderr, "Out of memory\n");
        fclose(fp);
        return 1;
    }

    size_t nread = fread(buf, 1, (size_t)size, fp);
    buf[nread] = '\0';
    fclose(fp);

    if (token_mode) {
        /* --tokens mode: dump token stream */
        css_tokenizer *tokenizer = css_tokenizer_create(buf, nread);
        if (!tokenizer) {
            fprintf(stderr, "Failed to create tokenizer\n");
            free(buf);
            return 1;
        }

        for (;;) {
            css_token *tok = css_tokenizer_next(tokenizer);
            if (!tok) {
                fprintf(stderr, "Token allocation failed\n");
                break;
            }
            /* Print [line:col] prefix */
            printf("[%zu:%zu] ", tok->line, tok->column);

            /* Enhanced display for various token types */
            if (tok->type == CSS_TOKEN_IDENT) {
                printf("<ident \"%s\">\n", tok->value ? tok->value : "");
            } else if (tok->type == CSS_TOKEN_FUNCTION) {
                printf("<function \"%s\">\n", tok->value ? tok->value : "");
            } else if (tok->type == CSS_TOKEN_AT_KEYWORD) {
                printf("<at-keyword \"%s\">\n", tok->value ? tok->value : "");
            } else if (tok->type == CSS_TOKEN_HASH) {
                printf("<hash \"%s\"%s>\n", tok->value ? tok->value : "",
                       tok->hash_type == CSS_HASH_ID ? " id" : "");
            } else if (tok->type == CSS_TOKEN_NUMBER) {
                if (tok->number_type == CSS_NUM_INTEGER)
                    printf("<number %d>\n", (int)tok->numeric_value);
                else
                    printf("<number %g>\n", tok->numeric_value);
            } else if (tok->type == CSS_TOKEN_PERCENTAGE) {
                if (tok->number_type == CSS_NUM_INTEGER)
                    printf("<percentage %d>\n", (int)tok->numeric_value);
                else
                    printf("<percentage %g>\n", tok->numeric_value);
            } else if (tok->type == CSS_TOKEN_DIMENSION) {
                if (tok->number_type == CSS_NUM_INTEGER)
                    printf("<dimension %d \"%s\">\n", (int)tok->numeric_value, tok->unit ? tok->unit : "");
                else
                    printf("<dimension %g \"%s\">\n", tok->numeric_value, tok->unit ? tok->unit : "");
            } else if (tok->type == CSS_TOKEN_STRING) {
                printf("<string \"%s\">\n", tok->value ? tok->value : "");
            } else if (tok->type == CSS_TOKEN_BAD_STRING) {
                printf("<bad-string>\n");
            } else if (tok->type == CSS_TOKEN_URL) {
                printf("<url \"%s\">\n", tok->value ? tok->value : "");
            } else if (tok->type == CSS_TOKEN_BAD_URL) {
                printf("<bad-url>\n");
            } else if (tok->type == CSS_TOKEN_DELIM) {
                if (tok->delim_codepoint < 0x80)
                    printf("<delim '%c'>\n", (char)tok->delim_codepoint);
                else
                    printf("<delim U+%04X>\n", tok->delim_codepoint);
            } else {
                printf("<%s>\n", css_token_type_name(tok->type));
            }
            bool is_eof = (tok->type == CSS_TOKEN_EOF);
            css_token_free(tok);
            if (is_eof) break;
        }

        css_tokenizer_free(tokenizer);
    } else {
        /* Default mode: will do full parse later */
        printf("Read %zu bytes from %s\n", nread, filename);
        printf("TODO: tokenize and parse\n");
    }

    free(buf);
    return 0;
}
