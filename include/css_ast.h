#ifndef CSS_AST_H
#define CSS_AST_H

#include "css_token.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* Node types */
typedef enum {
    CSS_NODE_STYLESHEET,
    CSS_NODE_AT_RULE,
    CSS_NODE_QUALIFIED_RULE,
    CSS_NODE_DECLARATION,
    CSS_NODE_COMPONENT_VALUE,
    CSS_NODE_SIMPLE_BLOCK,
    CSS_NODE_FUNCTION
} css_node_type;

/* Forward declarations */
typedef struct css_component_value css_component_value;
typedef struct css_simple_block css_simple_block;
typedef struct css_function css_function;
typedef struct css_declaration css_declaration;
typedef struct css_at_rule css_at_rule;
typedef struct css_qualified_rule css_qualified_rule;
typedef struct css_rule css_rule;
typedef struct css_stylesheet css_stylesheet;

/* Component value (§5.3): union of preserved token / simple block / function */
struct css_component_value {
    css_node_type type;  /* COMPONENT_VALUE, SIMPLE_BLOCK, or FUNCTION */
    union {
        css_token *token;           /* preserved token */
        css_simple_block *block;    /* simple block */
        css_function *function;     /* function */
    } u;
};

/* Simple block (§5.4.8): { }, [ ], ( ) with contents */
struct css_simple_block {
    css_token_type associated_token;  /* opening token: {, [, ( */
    css_component_value **values;
    size_t value_count;
    size_t value_cap;
};

/* Function (§5.4.9): name( ... ) */
struct css_function {
    char *name;
    css_component_value **values;
    size_t value_count;
    size_t value_cap;
};

/* Declaration (§5.4.6): name: value !important */
struct css_declaration {
    char *name;
    css_component_value **values;
    size_t value_count;
    size_t value_cap;
    bool important;
};

/* At-rule (§5.4.2): @name prelude { block } or @name prelude ; */
struct css_at_rule {
    char *name;
    css_component_value **prelude;
    size_t prelude_count;
    size_t prelude_cap;
    css_simple_block *block;  /* may be NULL for statement at-rules */
};

/* Qualified rule (§5.4.3): prelude { block } */
struct css_qualified_rule {
    css_component_value **prelude;
    size_t prelude_count;
    size_t prelude_cap;
    css_simple_block *block;
};

/* Rule: union wrapper for at-rule or qualified rule */
struct css_rule {
    css_node_type type;  /* AT_RULE or QUALIFIED_RULE */
    union {
        css_at_rule *at_rule;
        css_qualified_rule *qualified_rule;
    } u;
};

/* Stylesheet: top-level node */
struct css_stylesheet {
    css_rule **rules;
    size_t rule_count;
    size_t rule_cap;
};

/* === Creation functions === */
css_stylesheet *css_stylesheet_create(void);
css_rule *css_rule_create_at(css_at_rule *ar);
css_rule *css_rule_create_qualified(css_qualified_rule *qr);
css_at_rule *css_at_rule_create(const char *name);
css_qualified_rule *css_qualified_rule_create(void);
css_declaration *css_declaration_create(const char *name);
css_simple_block *css_simple_block_create(css_token_type associated);
css_function *css_function_create(const char *name);
css_component_value *css_component_value_create_token(css_token *token);
css_component_value *css_component_value_create_block(css_simple_block *block);
css_component_value *css_component_value_create_function(css_function *func);

/* === Free functions === */
void css_stylesheet_free(css_stylesheet *sheet);
void css_rule_free(css_rule *rule);
void css_at_rule_free(css_at_rule *ar);
void css_qualified_rule_free(css_qualified_rule *qr);
void css_declaration_free(css_declaration *decl);
void css_simple_block_free(css_simple_block *block);
void css_function_free(css_function *func);
void css_component_value_free(css_component_value *cv);

/* === Append helpers (dynamic arrays) === */
void css_stylesheet_append_rule(css_stylesheet *sheet, css_rule *rule);
void css_at_rule_append_prelude(css_at_rule *ar, css_component_value *cv);
void css_qualified_rule_append_prelude(css_qualified_rule *qr, css_component_value *cv);
void css_simple_block_append_value(css_simple_block *block, css_component_value *cv);
void css_function_append_value(css_function *func, css_component_value *cv);
void css_declaration_append_value(css_declaration *decl, css_component_value *cv);

/* === Dump (debug output) === */
void css_ast_dump(css_stylesheet *sheet, FILE *out);

#endif /* CSS_AST_H */
