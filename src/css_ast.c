#define _POSIX_C_SOURCE 200809L

#include "css_ast.h"
#include "css_selector.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Creation functions
 * ================================================================ */

css_stylesheet *css_stylesheet_create(void)
{
    css_stylesheet *sheet = calloc(1, sizeof(css_stylesheet));
    return sheet;
}

css_rule *css_rule_create_at(css_at_rule *ar)
{
    css_rule *rule = calloc(1, sizeof(css_rule));
    if (!rule) return NULL;
    rule->type = CSS_NODE_AT_RULE;
    rule->u.at_rule = ar;
    return rule;
}

css_rule *css_rule_create_qualified(css_qualified_rule *qr)
{
    css_rule *rule = calloc(1, sizeof(css_rule));
    if (!rule) return NULL;
    rule->type = CSS_NODE_QUALIFIED_RULE;
    rule->u.qualified_rule = qr;
    return rule;
}

css_at_rule *css_at_rule_create(const char *name)
{
    css_at_rule *ar = calloc(1, sizeof(css_at_rule));
    if (!ar) return NULL;
    if (name) ar->name = strdup(name);
    return ar;
}

css_qualified_rule *css_qualified_rule_create(void)
{
    css_qualified_rule *qr = calloc(1, sizeof(css_qualified_rule));
    return qr;
}

css_declaration *css_declaration_create(const char *name)
{
    css_declaration *decl = calloc(1, sizeof(css_declaration));
    if (!decl) return NULL;
    if (name) decl->name = strdup(name);
    return decl;
}

css_simple_block *css_simple_block_create(css_token_type associated)
{
    css_simple_block *block = calloc(1, sizeof(css_simple_block));
    if (!block) return NULL;
    block->associated_token = associated;
    return block;
}

css_function *css_function_create(const char *name)
{
    css_function *func = calloc(1, sizeof(css_function));
    if (!func) return NULL;
    if (name) func->name = strdup(name);
    return func;
}

css_component_value *css_component_value_create_token(css_token *token)
{
    css_component_value *cv = calloc(1, sizeof(css_component_value));
    if (!cv) return NULL;
    cv->type = CSS_NODE_COMPONENT_VALUE;
    cv->u.token = token;
    return cv;
}

css_component_value *css_component_value_create_block(css_simple_block *block)
{
    css_component_value *cv = calloc(1, sizeof(css_component_value));
    if (!cv) return NULL;
    cv->type = CSS_NODE_SIMPLE_BLOCK;
    cv->u.block = block;
    return cv;
}

css_component_value *css_component_value_create_function(css_function *func)
{
    css_component_value *cv = calloc(1, sizeof(css_component_value));
    if (!cv) return NULL;
    cv->type = CSS_NODE_FUNCTION;
    cv->u.function = func;
    return cv;
}

/* ================================================================
 * Free functions (all NULL-safe)
 * ================================================================ */

void css_component_value_free(css_component_value *cv)
{
    if (!cv) return;
    switch (cv->type) {
    case CSS_NODE_COMPONENT_VALUE:
        css_token_free(cv->u.token);
        break;
    case CSS_NODE_SIMPLE_BLOCK:
        css_simple_block_free(cv->u.block);
        break;
    case CSS_NODE_FUNCTION:
        css_function_free(cv->u.function);
        break;
    default:
        break;
    }
    free(cv);
}

void css_simple_block_free(css_simple_block *block)
{
    if (!block) return;
    for (size_t i = 0; i < block->value_count; i++) {
        css_component_value_free(block->values[i]);
    }
    free(block->values);
    free(block);
}

void css_function_free(css_function *func)
{
    if (!func) return;
    free(func->name);
    for (size_t i = 0; i < func->value_count; i++) {
        css_component_value_free(func->values[i]);
    }
    free(func->values);
    free(func);
}

void css_declaration_free(css_declaration *decl)
{
    if (!decl) return;
    free(decl->name);
    for (size_t i = 0; i < decl->value_count; i++) {
        css_component_value_free(decl->values[i]);
    }
    free(decl->values);
    free(decl);
}

void css_at_rule_free(css_at_rule *ar)
{
    if (!ar) return;
    free(ar->name);
    for (size_t i = 0; i < ar->prelude_count; i++) {
        css_component_value_free(ar->prelude[i]);
    }
    free(ar->prelude);
    css_simple_block_free(ar->block);
    free(ar);
}

void css_qualified_rule_free(css_qualified_rule *qr)
{
    if (!qr) return;
    for (size_t i = 0; i < qr->prelude_count; i++) {
        css_component_value_free(qr->prelude[i]);
    }
    free(qr->prelude);
    css_simple_block_free(qr->block);
    css_selector_list_free(qr->selectors);
    free(qr);
}

void css_rule_free(css_rule *rule)
{
    if (!rule) return;
    switch (rule->type) {
    case CSS_NODE_AT_RULE:
        css_at_rule_free(rule->u.at_rule);
        break;
    case CSS_NODE_QUALIFIED_RULE:
        css_qualified_rule_free(rule->u.qualified_rule);
        break;
    default:
        break;
    }
    free(rule);
}

void css_stylesheet_free(css_stylesheet *sheet)
{
    if (!sheet) return;
    for (size_t i = 0; i < sheet->rule_count; i++) {
        css_rule_free(sheet->rules[i]);
    }
    free(sheet->rules);
    free(sheet);
}

/* ================================================================
 * Append helpers (dynamic arrays with realloc)
 * ================================================================ */

void css_stylesheet_append_rule(css_stylesheet *sheet, css_rule *rule)
{
    if (!sheet || !rule) return;
    if (sheet->rule_count >= sheet->rule_cap) {
        sheet->rule_cap = sheet->rule_cap ? sheet->rule_cap * 2 : 4;
        sheet->rules = realloc(sheet->rules,
                               sheet->rule_cap * sizeof(css_rule *));
    }
    sheet->rules[sheet->rule_count++] = rule;
}

void css_at_rule_append_prelude(css_at_rule *ar, css_component_value *cv)
{
    if (!ar || !cv) return;
    if (ar->prelude_count >= ar->prelude_cap) {
        ar->prelude_cap = ar->prelude_cap ? ar->prelude_cap * 2 : 4;
        ar->prelude = realloc(ar->prelude,
                              ar->prelude_cap * sizeof(css_component_value *));
    }
    ar->prelude[ar->prelude_count++] = cv;
}

void css_qualified_rule_append_prelude(css_qualified_rule *qr,
                                       css_component_value *cv)
{
    if (!qr || !cv) return;
    if (qr->prelude_count >= qr->prelude_cap) {
        qr->prelude_cap = qr->prelude_cap ? qr->prelude_cap * 2 : 4;
        qr->prelude = realloc(qr->prelude,
                              qr->prelude_cap * sizeof(css_component_value *));
    }
    qr->prelude[qr->prelude_count++] = cv;
}

void css_simple_block_append_value(css_simple_block *block,
                                    css_component_value *cv)
{
    if (!block || !cv) return;
    if (block->value_count >= block->value_cap) {
        block->value_cap = block->value_cap ? block->value_cap * 2 : 4;
        block->values = realloc(block->values,
                                block->value_cap * sizeof(css_component_value *));
    }
    block->values[block->value_count++] = cv;
}

void css_function_append_value(css_function *func, css_component_value *cv)
{
    if (!func || !cv) return;
    if (func->value_count >= func->value_cap) {
        func->value_cap = func->value_cap ? func->value_cap * 2 : 4;
        func->values = realloc(func->values,
                               func->value_cap * sizeof(css_component_value *));
    }
    func->values[func->value_count++] = cv;
}

void css_declaration_append_value(css_declaration *decl, css_component_value *cv)
{
    if (!decl || !cv) return;
    if (decl->value_count >= decl->value_cap) {
        decl->value_cap = decl->value_cap ? decl->value_cap * 2 : 4;
        decl->values = realloc(decl->values,
                               decl->value_cap * sizeof(css_component_value *));
    }
    decl->values[decl->value_count++] = cv;
}

/* ================================================================
 * Dump (debug output)
 * ================================================================ */

/* Print indentation: depth levels of "  " (two spaces each) */
static void dump_indent(FILE *out, int depth)
{
    for (int i = 0; i < depth; i++) {
        fprintf(out, "  ");
    }
}

/* Format a token for inline display */
static void dump_token_inline(FILE *out, css_token *tok)
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

static void dump_component_value(FILE *out, css_component_value *cv,
                                  int depth);
static void dump_simple_block(FILE *out, css_simple_block *block, int depth);

static void dump_component_value(FILE *out, css_component_value *cv,
                                  int depth)
{
    if (!cv) return;
    switch (cv->type) {
    case CSS_NODE_COMPONENT_VALUE:
        dump_indent(out, depth);
        dump_token_inline(out, cv->u.token);
        fprintf(out, "\n");
        break;
    case CSS_NODE_SIMPLE_BLOCK:
        dump_simple_block(out, cv->u.block, depth);
        break;
    case CSS_NODE_FUNCTION:
        dump_indent(out, depth);
        fprintf(out, "FUNCTION \"%s\"\n",
                cv->u.function ? (cv->u.function->name ?
                cv->u.function->name : "") : "");
        if (cv->u.function) {
            for (size_t i = 0; i < cv->u.function->value_count; i++) {
                dump_component_value(out, cv->u.function->values[i],
                                     depth + 1);
            }
        }
        break;
    default:
        dump_indent(out, depth);
        fprintf(out, "<unknown node type %d>\n", cv->type);
        break;
    }
}

static void dump_simple_block(FILE *out, css_simple_block *block, int depth)
{
    if (!block) return;
    dump_indent(out, depth);
    char open = '?';
    switch (block->associated_token) {
    case CSS_TOKEN_OPEN_CURLY:  open = '{'; break;
    case CSS_TOKEN_OPEN_SQUARE: open = '['; break;
    case CSS_TOKEN_OPEN_PAREN:  open = '('; break;
    default: break;
    }
    char close = '?';
    switch (block->associated_token) {
    case CSS_TOKEN_OPEN_CURLY:  close = '}'; break;
    case CSS_TOKEN_OPEN_SQUARE: close = ']'; break;
    case CSS_TOKEN_OPEN_PAREN:  close = ')'; break;
    default: break;
    }
    fprintf(out, "BLOCK %c%c\n", open, close);
    for (size_t i = 0; i < block->value_count; i++) {
        dump_component_value(out, block->values[i], depth + 1);
    }
}

/* Used by parser dump in future tasks; suppress unused warning for now */
static void dump_declaration(FILE *out, css_declaration *decl, int depth)
    __attribute__((unused));
static void dump_declaration(FILE *out, css_declaration *decl, int depth)
{
    if (!decl) return;
    dump_indent(out, depth);
    fprintf(out, "DECLARATION \"%s\"", decl->name ? decl->name : "");
    if (decl->important) fprintf(out, " !important");
    fprintf(out, "\n");
    for (size_t i = 0; i < decl->value_count; i++) {
        dump_component_value(out, decl->values[i], depth + 1);
    }
}

static void dump_rule(FILE *out, css_rule *rule, int depth)
{
    if (!rule) return;
    switch (rule->type) {
    case CSS_NODE_AT_RULE: {
        css_at_rule *ar = rule->u.at_rule;
        if (!ar) break;
        dump_indent(out, depth);
        fprintf(out, "AT_RULE \"%s\"\n", ar->name ? ar->name : "");
        if (ar->prelude_count > 0) {
            dump_indent(out, depth + 1);
            fprintf(out, "prelude:\n");
            for (size_t i = 0; i < ar->prelude_count; i++) {
                dump_component_value(out, ar->prelude[i], depth + 2);
            }
        }
        if (ar->block) {
            dump_simple_block(out, ar->block, depth + 1);
        }
        break;
    }
    case CSS_NODE_QUALIFIED_RULE: {
        css_qualified_rule *qr = rule->u.qualified_rule;
        if (!qr) break;
        dump_indent(out, depth);
        fprintf(out, "QUALIFIED_RULE\n");
        if (qr->prelude_count > 0) {
            dump_indent(out, depth + 1);
            fprintf(out, "prelude:\n");
            for (size_t i = 0; i < qr->prelude_count; i++) {
                dump_component_value(out, qr->prelude[i], depth + 2);
            }
        }
        if (qr->block) {
            dump_simple_block(out, qr->block, depth + 1);
        }
        break;
    }
    default:
        dump_indent(out, depth);
        fprintf(out, "<unknown rule type %d>\n", rule->type);
        break;
    }
}

void css_ast_dump(css_stylesheet *sheet, FILE *out)
{
    if (!sheet || !out) return;
    fprintf(out, "STYLESHEET\n");
    for (size_t i = 0; i < sheet->rule_count; i++) {
        dump_rule(out, sheet->rules[i], 1);
    }
}
