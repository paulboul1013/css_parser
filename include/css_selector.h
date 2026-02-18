#ifndef CSS_SELECTOR_H
#define CSS_SELECTOR_H

#include "css_ast.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* ================================================================
 * Simple selector types (CSS Selectors Level 4)
 * ================================================================ */
typedef enum {
    SEL_TYPE,             /* element type selector, e.g. div, p */
    SEL_UNIVERSAL,        /* universal selector * */
    SEL_CLASS,            /* class selector .foo */
    SEL_ID,               /* id selector #bar */
    SEL_ATTRIBUTE,        /* attribute selector [href] */
    SEL_PSEUDO_CLASS,     /* pseudo-class :hover */
    SEL_PSEUDO_ELEMENT    /* pseudo-element ::before */
} css_simple_selector_type;

/* ================================================================
 * Attribute match operators
 * ================================================================ */
typedef enum {
    ATTR_EXISTS,          /* [attr]       — attribute exists */
    ATTR_EXACT,           /* [attr=val]   — exact match */
    ATTR_INCLUDES,        /* [attr~=val]  — space-separated list contains */
    ATTR_DASH,            /* [attr|=val]  — exact or prefix-dash */
    ATTR_PREFIX,          /* [attr^=val]  — starts with */
    ATTR_SUFFIX,          /* [attr$=val]  — ends with */
    ATTR_SUBSTRING        /* [attr*=val]  — contains substring */
} css_attr_match;

/* ================================================================
 * Combinator types
 * ================================================================ */
typedef enum {
    COMB_DESCENDANT,          /* ' '  descendant */
    COMB_CHILD,               /* '>'  child */
    COMB_NEXT_SIBLING,        /* '+'  adjacent sibling */
    COMB_SUBSEQUENT_SIBLING   /* '~'  general sibling */
} css_combinator;

/* ================================================================
 * Specificity (a, b, c)
 * ================================================================ */
typedef struct {
    unsigned int a;   /* #id count */
    unsigned int b;   /* .class, [attr], :pseudo-class count */
    unsigned int c;   /* type, ::pseudo-element count */
} css_specificity;

/* ================================================================
 * Simple selector
 * ================================================================ */
typedef struct {
    css_simple_selector_type type;
    char *name;                  /* element name, class name, id name,
                                    pseudo-class/element name (NULL for universal) */
    css_attr_match attr_match;   /* attribute match operator (SEL_ATTRIBUTE only) */
    char *attr_name;             /* attribute name (SEL_ATTRIBUTE only) */
    char *attr_value;            /* attribute value (SEL_ATTRIBUTE only, NULL for EXISTS) */
    bool attr_case_insensitive;  /* [attr=val i] case-insensitive flag */
} css_simple_selector;

/* ================================================================
 * Compound selector: sequence of simple selectors (no combinator)
 * e.g. div.foo#bar
 * ================================================================ */
typedef struct {
    css_simple_selector **selectors;
    size_t count;
    size_t cap;
} css_compound_selector;

/* ================================================================
 * Complex selector: compound selectors joined by combinators
 * e.g. div > .foo + p
 *
 * compounds[0] COMB combinators[0] compounds[1] COMB combinators[1] ...
 * combinators array has (count - 1) entries when count > 0.
 * ================================================================ */
typedef struct {
    css_compound_selector **compounds;
    css_combinator *combinators;   /* combinators[i] sits between compounds[i] and compounds[i+1] */
    size_t count;                  /* number of compound selectors */
    size_t cap;
} css_complex_selector;

/* ================================================================
 * Selector list: comma-separated complex selectors
 * e.g. div > .foo, #bar
 * ================================================================ */
typedef struct css_selector_list {
    css_complex_selector **selectors;
    size_t count;
    size_t cap;
} css_selector_list;

/* ================================================================
 * Lifecycle: create / free
 * ================================================================ */
css_simple_selector   *css_simple_selector_create(css_simple_selector_type type);
void                   css_simple_selector_free(css_simple_selector *sel);

css_compound_selector *css_compound_selector_create(void);
void                   css_compound_selector_free(css_compound_selector *comp);
void                   css_compound_selector_append(css_compound_selector *comp,
                                                    css_simple_selector *sel);

css_complex_selector  *css_complex_selector_create(void);
void                   css_complex_selector_free(css_complex_selector *cx);
void                   css_complex_selector_append(css_complex_selector *cx,
                                                   css_compound_selector *comp,
                                                   css_combinator comb);

css_selector_list     *css_selector_list_create(void);
void                   css_selector_list_free(css_selector_list *list);
void                   css_selector_list_append(css_selector_list *list,
                                                css_complex_selector *cx);

/* ================================================================
 * Parsing (stub until Task 4-5)
 * ================================================================ */
css_selector_list *css_parse_selector_list(css_component_value **values,
                                           size_t count);

/* ================================================================
 * Specificity (stub until Task 6)
 * ================================================================ */
css_specificity css_selector_specificity(css_complex_selector *sel);

/* ================================================================
 * Dump (debug output)
 * ================================================================ */
void css_selector_dump(css_selector_list *list, FILE *out, int depth);

#endif /* CSS_SELECTOR_H */
