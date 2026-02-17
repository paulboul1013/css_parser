#define _POSIX_C_SOURCE 200809L

#include "css_ast.h"
#include "css_token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_stylesheet_create_free(void)
{
    printf("  test_stylesheet_create_free...");
    css_stylesheet *sheet = css_stylesheet_create();
    assert(sheet != NULL);
    assert(sheet->rule_count == 0);
    assert(sheet->rules == NULL);
    css_stylesheet_free(sheet);
    printf(" OK\n");
}

static void test_component_value_token(void)
{
    printf("  test_component_value_token...");
    css_token *tok = css_token_create(CSS_TOKEN_IDENT);
    tok->value = strdup("body");
    css_component_value *cv = css_component_value_create_token(tok);
    assert(cv != NULL);
    assert(cv->type == CSS_NODE_COMPONENT_VALUE);
    assert(cv->u.token == tok);
    css_component_value_free(cv);  /* should also free the token */
    printf(" OK\n");
}

static void test_simple_block(void)
{
    printf("  test_simple_block...");
    css_simple_block *block = css_simple_block_create(CSS_TOKEN_OPEN_CURLY);
    assert(block != NULL);
    assert(block->associated_token == CSS_TOKEN_OPEN_CURLY);
    assert(block->value_count == 0);

    /* Append some values */
    for (int i = 0; i < 10; i++) {
        css_token *tok = css_token_create(CSS_TOKEN_WHITESPACE);
        css_component_value *cv = css_component_value_create_token(tok);
        css_simple_block_append_value(block, cv);
    }
    assert(block->value_count == 10);
    assert(block->value_cap >= 10);

    css_simple_block_free(block);
    printf(" OK\n");
}

static void test_function(void)
{
    printf("  test_function...");
    css_function *func = css_function_create("rgb");
    assert(func != NULL);
    assert(strcmp(func->name, "rgb") == 0);

    css_token *tok = css_token_create(CSS_TOKEN_NUMBER);
    tok->numeric_value = 255;
    css_component_value *cv = css_component_value_create_token(tok);
    css_function_append_value(func, cv);
    assert(func->value_count == 1);

    css_function_free(func);
    printf(" OK\n");
}

static void test_declaration(void)
{
    printf("  test_declaration...");
    css_declaration *decl = css_declaration_create("color");
    assert(decl != NULL);
    assert(strcmp(decl->name, "color") == 0);
    assert(decl->important == false);

    decl->important = true;

    css_token *tok = css_token_create(CSS_TOKEN_IDENT);
    tok->value = strdup("red");
    css_component_value *cv = css_component_value_create_token(tok);
    css_declaration_append_value(decl, cv);
    assert(decl->value_count == 1);

    css_declaration_free(decl);
    printf(" OK\n");
}

static void test_qualified_rule(void)
{
    printf("  test_qualified_rule...");
    css_qualified_rule *qr = css_qualified_rule_create();
    assert(qr != NULL);

    /* Add prelude: ident "body" */
    css_token *tok = css_token_create(CSS_TOKEN_IDENT);
    tok->value = strdup("body");
    css_component_value *cv = css_component_value_create_token(tok);
    css_qualified_rule_append_prelude(qr, cv);
    assert(qr->prelude_count == 1);

    /* Add block */
    css_simple_block *block = css_simple_block_create(CSS_TOKEN_OPEN_CURLY);
    qr->block = block;

    css_qualified_rule_free(qr);
    printf(" OK\n");
}

static void test_at_rule(void)
{
    printf("  test_at_rule...");
    css_at_rule *ar = css_at_rule_create("media");
    assert(ar != NULL);
    assert(strcmp(ar->name, "media") == 0);
    assert(ar->block == NULL);

    /* Statement at-rule (no block) */
    css_at_rule_free(ar);

    /* At-rule with block */
    ar = css_at_rule_create("media");
    ar->block = css_simple_block_create(CSS_TOKEN_OPEN_CURLY);
    css_at_rule_free(ar);
    printf(" OK\n");
}

static void test_rule_wrappers(void)
{
    printf("  test_rule_wrappers...");
    /* At-rule wrapper */
    css_at_rule *ar = css_at_rule_create("import");
    css_rule *r1 = css_rule_create_at(ar);
    assert(r1->type == CSS_NODE_AT_RULE);
    assert(r1->u.at_rule == ar);
    css_rule_free(r1);

    /* Qualified rule wrapper */
    css_qualified_rule *qr = css_qualified_rule_create();
    css_rule *r2 = css_rule_create_qualified(qr);
    assert(r2->type == CSS_NODE_QUALIFIED_RULE);
    assert(r2->u.qualified_rule == qr);
    css_rule_free(r2);
    printf(" OK\n");
}

static void test_stylesheet_with_rules(void)
{
    printf("  test_stylesheet_with_rules...");
    css_stylesheet *sheet = css_stylesheet_create();

    /* Add a qualified rule: body { } */
    css_qualified_rule *qr = css_qualified_rule_create();
    css_token *tok = css_token_create(CSS_TOKEN_IDENT);
    tok->value = strdup("body");
    css_qualified_rule_append_prelude(qr, css_component_value_create_token(tok));
    qr->block = css_simple_block_create(CSS_TOKEN_OPEN_CURLY);
    css_stylesheet_append_rule(sheet, css_rule_create_qualified(qr));

    /* Add an at-rule: @import url(...); */
    css_at_rule *ar = css_at_rule_create("import");
    css_stylesheet_append_rule(sheet, css_rule_create_at(ar));

    assert(sheet->rule_count == 2);

    css_stylesheet_free(sheet);
    printf(" OK\n");
}

static void test_dump(void)
{
    printf("  test_dump...");
    css_stylesheet *sheet = css_stylesheet_create();

    /* body { color: red } — build manually */
    css_qualified_rule *qr = css_qualified_rule_create();
    css_token *tok_body = css_token_create(CSS_TOKEN_IDENT);
    tok_body->value = strdup("body");
    css_qualified_rule_append_prelude(qr,
        css_component_value_create_token(tok_body));

    css_simple_block *block = css_simple_block_create(CSS_TOKEN_OPEN_CURLY);

    /* Put an ident "color" inside the block as component value */
    css_token *tok_color = css_token_create(CSS_TOKEN_IDENT);
    tok_color->value = strdup("color");
    css_simple_block_append_value(block,
        css_component_value_create_token(tok_color));

    css_token *tok_red = css_token_create(CSS_TOKEN_IDENT);
    tok_red->value = strdup("red");
    css_simple_block_append_value(block,
        css_component_value_create_token(tok_red));

    qr->block = block;
    css_stylesheet_append_rule(sheet, css_rule_create_qualified(qr));

    /* @media with block */
    css_at_rule *ar = css_at_rule_create("media");
    css_token *tok_screen = css_token_create(CSS_TOKEN_IDENT);
    tok_screen->value = strdup("screen");
    css_at_rule_append_prelude(ar,
        css_component_value_create_token(tok_screen));
    ar->block = css_simple_block_create(CSS_TOKEN_OPEN_CURLY);
    css_stylesheet_append_rule(sheet, css_rule_create_at(ar));

    printf("\n--- AST dump ---\n");
    css_ast_dump(sheet, stdout);
    printf("--- end dump ---\n");

    css_stylesheet_free(sheet);
    printf("  test_dump... OK\n");
}

static void test_null_safety(void)
{
    printf("  test_null_safety...");
    /* All free functions should handle NULL gracefully */
    css_stylesheet_free(NULL);
    css_rule_free(NULL);
    css_at_rule_free(NULL);
    css_qualified_rule_free(NULL);
    css_declaration_free(NULL);
    css_simple_block_free(NULL);
    css_function_free(NULL);
    css_component_value_free(NULL);

    /* Append with NULL should not crash */
    css_stylesheet_append_rule(NULL, NULL);
    css_simple_block_append_value(NULL, NULL);
    css_function_append_value(NULL, NULL);
    css_declaration_append_value(NULL, NULL);
    css_at_rule_append_prelude(NULL, NULL);
    css_qualified_rule_append_prelude(NULL, NULL);

    /* Dump with NULL should not crash */
    css_ast_dump(NULL, stdout);

    printf(" OK\n");
}

int main(void)
{
    printf("=== AST unit tests ===\n");
    test_stylesheet_create_free();
    test_component_value_token();
    test_simple_block();
    test_function();
    test_declaration();
    test_qualified_rule();
    test_at_rule();
    test_rule_wrappers();
    test_stylesheet_with_rules();
    test_dump();
    test_null_safety();
    printf("=== All AST tests passed ===\n");
    return 0;
}
