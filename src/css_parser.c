#define _POSIX_C_SOURCE 200809L

#include "css_parser.h"
#include "css_tokenizer.h"
#include "css_ast.h"
#include "css_selector.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>  /* strcasecmp */

/* ================================================================
 * Internal parser struct
 * ================================================================ */

typedef struct {
    css_tokenizer *tokenizer;
    css_token *current_token;  /* currently consumed token (owned) */
    bool reconsume;
} css_parser_ctx;

/* ================================================================
 * Token consumption helpers
 * ================================================================ */

static css_token *next_token(css_parser_ctx *p)
{
    if (p->reconsume) {
        p->reconsume = false;
        return p->current_token;
    }
    if (p->current_token) {
        css_token_free(p->current_token);
    }
    p->current_token = css_tokenizer_next(p->tokenizer);
    return p->current_token;
}

static void reconsume(css_parser_ctx *p)
{
    p->reconsume = true;
}

/* ================================================================
 * Token cloning helper
 * ================================================================ */

static css_token *clone_token(css_token *src)
{
    if (!src) return NULL;
    css_token *dst = css_token_create(src->type);
    if (!dst) return NULL;
    dst->value = src->value ? strdup(src->value) : NULL;
    dst->numeric_value = src->numeric_value;
    dst->number_type = src->number_type;
    dst->unit = src->unit ? strdup(src->unit) : NULL;
    dst->hash_type = src->hash_type;
    dst->delim_codepoint = src->delim_codepoint;
    dst->line = src->line;
    dst->column = src->column;
    return dst;
}

/* ================================================================
 * Forward declarations
 * ================================================================ */

static css_component_value *consume_component_value(css_parser_ctx *p);
static css_simple_block *consume_simple_block(css_parser_ctx *p);
static css_function *consume_function(css_parser_ctx *p);

/* ================================================================
 * consume_component_value (CSS Syntax §5.4.7)
 * ================================================================ */

static css_component_value *consume_component_value(css_parser_ctx *p)
{
    css_token *tok = next_token(p);

    if (tok->type == CSS_TOKEN_OPEN_CURLY ||
        tok->type == CSS_TOKEN_OPEN_SQUARE ||
        tok->type == CSS_TOKEN_OPEN_PAREN) {
        css_simple_block *block = consume_simple_block(p);
        return css_component_value_create_block(block);
    }

    if (tok->type == CSS_TOKEN_FUNCTION) {
        css_function *func = consume_function(p);
        return css_component_value_create_function(func);
    }

    /* Preserved token — clone since current_token is owned by parser */
    css_token *copy = clone_token(tok);
    return css_component_value_create_token(copy);
}

/* ================================================================
 * consume_simple_block (CSS Syntax §5.4.8)
 * ================================================================ */

static css_simple_block *consume_simple_block(css_parser_ctx *p)
{
    /* Current token is {, [, or ( */
    css_token_type open = p->current_token->type;
    css_token_type mirror;

    if (open == CSS_TOKEN_OPEN_CURLY)
        mirror = CSS_TOKEN_CLOSE_CURLY;
    else if (open == CSS_TOKEN_OPEN_SQUARE)
        mirror = CSS_TOKEN_CLOSE_SQUARE;
    else
        mirror = CSS_TOKEN_CLOSE_PAREN;

    css_simple_block *block = css_simple_block_create(open);

    for (;;) {
        css_token *tok = next_token(p);
        if (tok->type == mirror) {
            return block;
        }
        if (tok->type == CSS_TOKEN_EOF) {
            /* Parse error — return what we have */
            return block;
        }
        reconsume(p);
        css_component_value *cv = consume_component_value(p);
        css_simple_block_append_value(block, cv);
    }
}

/* ================================================================
 * consume_function (CSS Syntax §5.4.9)
 * ================================================================ */

static css_function *consume_function(css_parser_ctx *p)
{
    /* Current token is function-token */
    css_function *func = css_function_create(p->current_token->value);

    for (;;) {
        css_token *tok = next_token(p);
        if (tok->type == CSS_TOKEN_CLOSE_PAREN) {
            return func;
        }
        if (tok->type == CSS_TOKEN_EOF) {
            /* Parse error — return what we have */
            return func;
        }
        reconsume(p);
        css_component_value *cv = consume_component_value(p);
        css_function_append_value(func, cv);
    }
}

/* ================================================================
 * consume_at_rule (CSS Syntax §5.4.2)
 * ================================================================ */

static css_at_rule *consume_at_rule(css_parser_ctx *p)
{
    /* Current token is at-keyword-token */
    css_at_rule *ar = css_at_rule_create(p->current_token->value);

    for (;;) {
        css_token *tok = next_token(p);
        if (tok->type == CSS_TOKEN_SEMICOLON) {
            return ar;
        }
        if (tok->type == CSS_TOKEN_EOF) {
            /* Parse error */
            return ar;
        }
        if (tok->type == CSS_TOKEN_OPEN_CURLY) {
            ar->block = consume_simple_block(p);
            return ar;
        }
        reconsume(p);
        css_component_value *cv = consume_component_value(p);
        css_at_rule_append_prelude(ar, cv);
    }
}

/* ================================================================
 * consume_qualified_rule (CSS Syntax §5.4.3)
 * ================================================================ */

static css_qualified_rule *consume_qualified_rule(css_parser_ctx *p)
{
    css_qualified_rule *qr = css_qualified_rule_create();

    for (;;) {
        css_token *tok = next_token(p);
        if (tok->type == CSS_TOKEN_EOF) {
            /* Parse error — discard the rule */
            css_qualified_rule_free(qr);
            return NULL;
        }
        if (tok->type == CSS_TOKEN_OPEN_CURLY) {
            qr->block = consume_simple_block(p);
            return qr;
        }
        reconsume(p);
        css_component_value *cv = consume_component_value(p);
        css_qualified_rule_append_prelude(qr, cv);
    }
}

/* ================================================================
 * consume_list_of_rules (CSS Syntax §5.4.1)
 * ================================================================ */

static void consume_list_of_rules(css_parser_ctx *p, css_stylesheet *sheet,
                                  bool top_level)
{
    for (;;) {
        css_token *tok = next_token(p);
        if (tok->type == CSS_TOKEN_WHITESPACE) {
            continue;
        }
        if (tok->type == CSS_TOKEN_EOF) {
            return;
        }
        if (tok->type == CSS_TOKEN_CDO || tok->type == CSS_TOKEN_CDC) {
            if (top_level) continue;
            reconsume(p);
            css_qualified_rule *qr = consume_qualified_rule(p);
            if (qr) {
                css_stylesheet_append_rule(sheet,
                    css_rule_create_qualified(qr));
            }
            continue;
        }
        if (tok->type == CSS_TOKEN_AT_KEYWORD) {
            css_at_rule *ar = consume_at_rule(p);
            css_stylesheet_append_rule(sheet, css_rule_create_at(ar));
            continue;
        }
        reconsume(p);
        css_qualified_rule *qr = consume_qualified_rule(p);
        if (qr) {
            css_stylesheet_append_rule(sheet,
                css_rule_create_qualified(qr));
        }
    }
}

/* ================================================================
 * Post-processing: parse declarations from block contents
 *
 * Walk a simple block's component values and detect declaration
 * patterns: <ident> <whitespace>* <colon> <whitespace>* <values> <semicolon>
 *
 * This modifies the block in-place, replacing raw tokens with
 * DECLARATION component values where possible.
 * ================================================================ */

/* Helper: check if a component_value is a preserved token of given type */
static bool cv_is_token(css_component_value *cv, css_token_type type)
{
    return cv && cv->type == CSS_NODE_COMPONENT_VALUE &&
           cv->u.token && cv->u.token->type == type;
}

/* Helper: clone a component value (deep copy) */
static css_component_value *clone_cv(css_component_value *src);

static css_simple_block *clone_simple_block(css_simple_block *src)
{
    if (!src) return NULL;
    css_simple_block *dst = css_simple_block_create(src->associated_token);
    if (!dst) return NULL;
    for (size_t i = 0; i < src->value_count; i++) {
        css_component_value *cv = clone_cv(src->values[i]);
        if (cv) css_simple_block_append_value(dst, cv);
    }
    return dst;
}

static css_function *clone_function(css_function *src)
{
    if (!src) return NULL;
    css_function *dst = css_function_create(src->name);
    if (!dst) return NULL;
    for (size_t i = 0; i < src->value_count; i++) {
        css_component_value *cv = clone_cv(src->values[i]);
        if (cv) css_function_append_value(dst, cv);
    }
    return dst;
}

static css_component_value *clone_cv(css_component_value *src)
{
    if (!src) return NULL;
    switch (src->type) {
    case CSS_NODE_COMPONENT_VALUE:
        return css_component_value_create_token(clone_token(src->u.token));
    case CSS_NODE_SIMPLE_BLOCK:
        return css_component_value_create_block(
            clone_simple_block(src->u.block));
    case CSS_NODE_FUNCTION:
        return css_component_value_create_function(
            clone_function(src->u.function));
    default:
        return NULL;
    }
}

/* Check and set !important flag on declaration.
 * Last two non-whitespace values should be delim('!') + ident("important") */
static void check_important(css_declaration *decl)
{
    if (!decl || decl->value_count < 2) return;

    /* Find the last two non-whitespace values */
    size_t last_idx = decl->value_count;
    size_t important_idx = 0;
    size_t bang_idx = 0;
    bool found_important = false;
    bool found_bang = false;

    /* Scan from end, skip whitespace, find "important" ident */
    size_t i = decl->value_count;
    while (i > 0) {
        i--;
        if (cv_is_token(decl->values[i], CSS_TOKEN_WHITESPACE)) continue;
        if (!found_important) {
            if (cv_is_token(decl->values[i], CSS_TOKEN_IDENT) &&
                decl->values[i]->u.token->value) {
                /* Case-insensitive check for "important" */
                const char *v = decl->values[i]->u.token->value;
                if (strcasecmp(v, "important") == 0) {
                    found_important = true;
                    important_idx = i;
                    last_idx = i;
                    continue;
                }
            }
            return; /* Not !important */
        }
        if (!found_bang) {
            if (cv_is_token(decl->values[i], CSS_TOKEN_DELIM) &&
                decl->values[i]->u.token->delim_codepoint == '!') {
                found_bang = true;
                bang_idx = i;
                break;
            }
            return; /* Not !important */
        }
    }

    if (!found_important || !found_bang) return;

    decl->important = true;

    /* Remove the !important tokens and any trailing whitespace */
    /* Free from bang_idx to end, then trim trailing whitespace */
    for (size_t j = bang_idx; j < decl->value_count; j++) {
        css_component_value_free(decl->values[j]);
    }
    decl->value_count = bang_idx;

    /* Trim trailing whitespace */
    while (decl->value_count > 0) {
        css_component_value *cv = decl->values[decl->value_count - 1];
        if (cv_is_token(cv, CSS_TOKEN_WHITESPACE)) {
            css_component_value_free(cv);
            decl->value_count--;
        } else {
            break;
        }
    }

    (void)last_idx;
    (void)important_idx;
}

/* Parse declarations from a qualified rule's block.
 * Returns a dynamically allocated array of declarations.
 * The caller is responsible for freeing the declarations. */
static void parse_declarations_from_block(css_simple_block *block,
                                          css_declaration ***out_decls,
                                          size_t *out_count)
{
    *out_decls = NULL;
    *out_count = 0;

    if (!block) return;

    size_t cap = 0;
    size_t i = 0;

    while (i < block->value_count) {
        /* Skip whitespace */
        while (i < block->value_count &&
               cv_is_token(block->values[i], CSS_TOKEN_WHITESPACE)) {
            i++;
        }
        if (i >= block->value_count) break;

        /* Skip semicolons */
        if (cv_is_token(block->values[i], CSS_TOKEN_SEMICOLON)) {
            i++;
            continue;
        }

        /* Check for at-keyword (nested at-rules in block — skip for now) */
        if (cv_is_token(block->values[i], CSS_TOKEN_AT_KEYWORD)) {
            /* Skip to next semicolon or sub-block */
            while (i < block->value_count) {
                if (cv_is_token(block->values[i], CSS_TOKEN_SEMICOLON)) {
                    i++;
                    break;
                }
                if (block->values[i]->type == CSS_NODE_SIMPLE_BLOCK) {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        /* Expect ident for declaration name */
        if (!cv_is_token(block->values[i], CSS_TOKEN_IDENT)) {
            /* Error recovery: skip to next semicolon */
            while (i < block->value_count &&
                   !cv_is_token(block->values[i], CSS_TOKEN_SEMICOLON)) {
                i++;
            }
            continue;
        }

        const char *name = block->values[i]->u.token->value;
        size_t name_idx = i;
        i++;

        /* Skip whitespace */
        while (i < block->value_count &&
               cv_is_token(block->values[i], CSS_TOKEN_WHITESPACE)) {
            i++;
        }

        /* Expect colon */
        if (i >= block->value_count ||
            !cv_is_token(block->values[i], CSS_TOKEN_COLON)) {
            /* Error recovery: skip to next semicolon */
            i = name_idx + 1;
            while (i < block->value_count &&
                   !cv_is_token(block->values[i], CSS_TOKEN_SEMICOLON)) {
                i++;
            }
            continue;
        }
        i++; /* skip colon */

        /* Skip whitespace after colon */
        while (i < block->value_count &&
               cv_is_token(block->values[i], CSS_TOKEN_WHITESPACE)) {
            i++;
        }

        /* Create declaration */
        css_declaration *decl = css_declaration_create(name);
        if (!decl) continue;

        /* Collect values until semicolon or end of block */
        while (i < block->value_count &&
               !cv_is_token(block->values[i], CSS_TOKEN_SEMICOLON)) {
            css_component_value *cv = clone_cv(block->values[i]);
            if (cv) css_declaration_append_value(decl, cv);
            i++;
        }

        /* Trim trailing whitespace from values */
        while (decl->value_count > 0) {
            css_component_value *last = decl->values[decl->value_count - 1];
            if (cv_is_token(last, CSS_TOKEN_WHITESPACE)) {
                css_component_value_free(last);
                decl->value_count--;
            } else {
                break;
            }
        }

        /* Check for !important */
        check_important(decl);

        /* Add to output array */
        if (*out_count >= cap) {
            cap = cap ? cap * 2 : 4;
            *out_decls = realloc(*out_decls, cap * sizeof(css_declaration *));
        }
        (*out_decls)[(*out_count)++] = decl;
    }
}

/* ================================================================
 * css_parse_stylesheet (public API)
 * ================================================================ */

css_stylesheet *css_parse_stylesheet(const char *input, size_t length)
{
    css_parser_ctx parser;
    memset(&parser, 0, sizeof(parser));

    parser.tokenizer = css_tokenizer_create(input, length);
    if (!parser.tokenizer) return NULL;

    css_stylesheet *sheet = css_stylesheet_create();
    if (!sheet) {
        css_tokenizer_free(parser.tokenizer);
        return NULL;
    }

    consume_list_of_rules(&parser, sheet, true);

    /* Post-process: parse selectors from qualified rule preludes */
    for (size_t i = 0; i < sheet->rule_count; i++) {
        css_rule *rule = sheet->rules[i];
        if (rule && rule->type == CSS_NODE_QUALIFIED_RULE) {
            css_qualified_rule *qr = rule->u.qualified_rule;
            if (qr && qr->prelude_count > 0) {
                qr->selectors = css_parse_selector_list(
                    qr->prelude, qr->prelude_count);
            }
        }
    }

    /* Clean up parser state */
    if (parser.current_token) {
        css_token_free(parser.current_token);
    }
    css_tokenizer_free(parser.tokenizer);

    /* Post-process: parse declarations from qualified rule blocks.
     * We store the declarations in a format that css_ast_dump can
     * display. Since we cannot modify css_ast.h, we'll replace
     * the block contents with declaration-style component values.
     *
     * Actually, we leave the raw AST as-is. The dump function in
     * css_ast.c already shows the block contents. For a richer
     * display, we provide css_parse_stylesheet_dump() below. */

    return sheet;
}

/* ================================================================
 * Enhanced dump: parse declarations inline during dump
 * ================================================================ */

/* Forward declarations for dump helpers */
static void dump_indent_p(FILE *out, int depth)
{
    for (int i = 0; i < depth; i++) {
        fprintf(out, "  ");
    }
}

static void dump_token_inline_p(FILE *out, css_token *tok)
{
    if (!tok) {
        fprintf(out, "<null>");
        return;
    }
    switch (tok->type) {
    case CSS_TOKEN_IDENT:
        fprintf(out, "<ident \"%s\">", tok->value ? tok->value : "");
        break;
    case CSS_TOKEN_FUNCTION:
        fprintf(out, "<function \"%s\">", tok->value ? tok->value : "");
        break;
    case CSS_TOKEN_AT_KEYWORD:
        fprintf(out, "<at-keyword \"%s\">", tok->value ? tok->value : "");
        break;
    case CSS_TOKEN_HASH:
        fprintf(out, "<hash \"%s\"%s>", tok->value ? tok->value : "",
                tok->hash_type == CSS_HASH_ID ? " id" : "");
        break;
    case CSS_TOKEN_STRING:
        fprintf(out, "<string \"%s\">", tok->value ? tok->value : "");
        break;
    case CSS_TOKEN_URL:
        fprintf(out, "<url \"%s\">", tok->value ? tok->value : "");
        break;
    case CSS_TOKEN_NUMBER:
        if (tok->number_type == CSS_NUM_INTEGER)
            fprintf(out, "<number %d>", (int)tok->numeric_value);
        else
            fprintf(out, "<number %g>", tok->numeric_value);
        break;
    case CSS_TOKEN_PERCENTAGE:
        if (tok->number_type == CSS_NUM_INTEGER)
            fprintf(out, "<percentage %d>", (int)tok->numeric_value);
        else
            fprintf(out, "<percentage %g>", tok->numeric_value);
        break;
    case CSS_TOKEN_DIMENSION:
        if (tok->number_type == CSS_NUM_INTEGER)
            fprintf(out, "<dimension %d \"%s\">", (int)tok->numeric_value,
                    tok->unit ? tok->unit : "");
        else
            fprintf(out, "<dimension %g \"%s\">", tok->numeric_value,
                    tok->unit ? tok->unit : "");
        break;
    case CSS_TOKEN_DELIM:
        if (tok->delim_codepoint < 0x80)
            fprintf(out, "<delim '%c'>", (char)tok->delim_codepoint);
        else
            fprintf(out, "<delim U+%04X>", tok->delim_codepoint);
        break;
    case CSS_TOKEN_WHITESPACE:
        fprintf(out, "<whitespace>");
        break;
    default:
        fprintf(out, "<%s>", css_token_type_name(tok->type));
        break;
    }
}

static void dump_cv_p(FILE *out, css_component_value *cv, int depth);
static void dump_block_with_decls(FILE *out, css_simple_block *block,
                                  int depth);

static void dump_cv_p(FILE *out, css_component_value *cv, int depth)
{
    if (!cv) return;
    switch (cv->type) {
    case CSS_NODE_COMPONENT_VALUE:
        dump_indent_p(out, depth);
        dump_token_inline_p(out, cv->u.token);
        fprintf(out, "\n");
        break;
    case CSS_NODE_SIMPLE_BLOCK:
        dump_block_with_decls(out, cv->u.block, depth);
        break;
    case CSS_NODE_FUNCTION:
        dump_indent_p(out, depth);
        fprintf(out, "FUNCTION \"%s\"\n",
                cv->u.function ? (cv->u.function->name ?
                cv->u.function->name : "") : "");
        if (cv->u.function) {
            for (size_t i = 0; i < cv->u.function->value_count; i++) {
                dump_cv_p(out, cv->u.function->values[i], depth + 1);
            }
        }
        break;
    default:
        dump_indent_p(out, depth);
        fprintf(out, "<unknown node type %d>\n", cv->type);
        break;
    }
}

/* Dump a block with declaration detection for {} blocks */
static void dump_block_with_decls(FILE *out, css_simple_block *block,
                                  int depth)
{
    if (!block) return;
    dump_indent_p(out, depth);
    char open = '?', close = '?';
    switch (block->associated_token) {
    case CSS_TOKEN_OPEN_CURLY:  open = '{'; close = '}'; break;
    case CSS_TOKEN_OPEN_SQUARE: open = '['; close = ']'; break;
    case CSS_TOKEN_OPEN_PAREN:  open = '('; close = ')'; break;
    default: break;
    }
    fprintf(out, "BLOCK %c%c\n", open, close);

    /* For {} blocks, try to parse as declarations */
    if (block->associated_token == CSS_TOKEN_OPEN_CURLY) {
        css_declaration **decls = NULL;
        size_t decl_count = 0;
        parse_declarations_from_block(block, &decls, &decl_count);

        if (decl_count > 0) {
            for (size_t i = 0; i < decl_count; i++) {
                css_declaration *d = decls[i];
                dump_indent_p(out, depth + 1);
                fprintf(out, "DECLARATION \"%s\"",
                        d->name ? d->name : "");
                if (d->important) fprintf(out, " !important");
                fprintf(out, "\n");
                for (size_t j = 0; j < d->value_count; j++) {
                    dump_cv_p(out, d->values[j], depth + 2);
                }
                css_declaration_free(d);
            }
            free(decls);
            return;
        }
        free(decls);
        /* Fall through to raw dump if no declarations found */
    }

    /* Raw dump for non-{} blocks or blocks with no declarations */
    for (size_t i = 0; i < block->value_count; i++) {
        dump_cv_p(out, block->values[i], depth + 1);
    }
}

/* Enhanced dump for parsed AST with declaration detection */
void css_parse_dump(css_stylesheet *sheet, FILE *out)
{
    if (!sheet || !out) return;
    fprintf(out, "STYLESHEET\n");

    for (size_t i = 0; i < sheet->rule_count; i++) {
        css_rule *rule = sheet->rules[i];
        if (!rule) continue;

        switch (rule->type) {
        case CSS_NODE_AT_RULE: {
            css_at_rule *ar = rule->u.at_rule;
            if (!ar) break;
            dump_indent_p(out, 1);
            fprintf(out, "AT_RULE \"%s\"\n", ar->name ? ar->name : "");
            if (ar->prelude_count > 0) {
                dump_indent_p(out, 2);
                fprintf(out, "prelude:\n");
                for (size_t j = 0; j < ar->prelude_count; j++) {
                    dump_cv_p(out, ar->prelude[j], 3);
                }
            }
            if (ar->block) {
                dump_block_with_decls(out, ar->block, 2);
            }
            break;
        }
        case CSS_NODE_QUALIFIED_RULE: {
            css_qualified_rule *qr = rule->u.qualified_rule;
            if (!qr) break;
            dump_indent_p(out, 1);
            fprintf(out, "QUALIFIED_RULE\n");
            if (qr->selectors) {
                css_selector_dump(qr->selectors, out, 2);
            }
            if (qr->prelude_count > 0) {
                dump_indent_p(out, 2);
                fprintf(out, "prelude:\n");
                for (size_t j = 0; j < qr->prelude_count; j++) {
                    dump_cv_p(out, qr->prelude[j], 3);
                }
            }
            if (qr->block) {
                dump_block_with_decls(out, qr->block, 2);
            }
            break;
        }
        default:
            dump_indent_p(out, 1);
            fprintf(out, "<unknown rule type %d>\n", rule->type);
            break;
        }
    }
}
