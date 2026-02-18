# P2a Selector Parser Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Parse CSS selector syntax from qualified rule preludes into structured selector AST with specificity calculation.

**Architecture:** Walk existing prelude component values (tokens/blocks) to build a 4-level selector hierarchy: simple → compound → complex → list. Integrate into existing parser by adding a `selectors` field to `css_qualified_rule` and calling selector parsing in `css_parse_stylesheet()`.

**Tech Stack:** C11, zero dependencies, matches existing codebase patterns (calloc/strdup/realloc dynamic arrays).

---

### Task 1: Selector data structures (css_selector.h)

**Files:**
- Create: `include/css_selector.h`

**Step 1: Create header with all type definitions and API declarations**

```c
#ifndef CSS_SELECTOR_H
#define CSS_SELECTOR_H

#include "css_ast.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* Simple selector types */
typedef enum {
    SEL_TYPE,
    SEL_UNIVERSAL,
    SEL_CLASS,
    SEL_ID,
    SEL_ATTRIBUTE,
    SEL_PSEUDO_CLASS,
    SEL_PSEUDO_ELEMENT
} css_simple_selector_type;

/* Attribute match operators */
typedef enum {
    ATTR_EXISTS,
    ATTR_EXACT,
    ATTR_INCLUDES,
    ATTR_DASH_MATCH,
    ATTR_PREFIX,
    ATTR_SUFFIX,
    ATTR_SUBSTRING
} css_attr_match;

/* Combinators */
typedef enum {
    COMB_DESCENDANT,
    COMB_CHILD,
    COMB_NEXT_SIBLING,
    COMB_SUBSEQUENT_SIBLING
} css_combinator;

/* Specificity (a, b, c) */
typedef struct {
    unsigned int a;
    unsigned int b;
    unsigned int c;
} css_specificity;

/* Simple selector */
typedef struct {
    css_simple_selector_type type;
    char *name;

    /* attribute selector fields */
    css_attr_match attr_match;
    char *attr_name;
    char *attr_value;
    bool attr_case_insensitive;
} css_simple_selector;

/* Compound selector = group of simple selectors */
typedef struct {
    css_simple_selector **selectors;
    size_t count;
    size_t cap;
} css_compound_selector;

/* Complex selector = compound + combinator chain */
typedef struct {
    css_compound_selector **compounds;
    css_combinator *combinators;   /* between compounds[i] and compounds[i+1] */
    size_t count;                  /* number of compounds */
    size_t cap;
} css_complex_selector;

/* Selector list (comma-separated) */
typedef struct css_selector_list {
    css_complex_selector **selectors;
    size_t count;
    size_t cap;
} css_selector_list;

/* === Lifecycle === */
css_simple_selector *css_simple_selector_create(css_simple_selector_type type);
void css_simple_selector_free(css_simple_selector *sel);

css_compound_selector *css_compound_selector_create(void);
void css_compound_selector_free(css_compound_selector *sel);
void css_compound_selector_append(css_compound_selector *comp,
                                   css_simple_selector *simple);

css_complex_selector *css_complex_selector_create(void);
void css_complex_selector_free(css_complex_selector *sel);
void css_complex_selector_append(css_complex_selector *complex,
                                  css_compound_selector *compound,
                                  css_combinator comb);

css_selector_list *css_selector_list_create(void);
void css_selector_list_free(css_selector_list *list);
void css_selector_list_append(css_selector_list *list,
                               css_complex_selector *complex);

/* === Parsing === */
css_selector_list *css_parse_selector_list(css_component_value **values,
                                            size_t count);

/* === Specificity === */
css_specificity css_selector_specificity(css_complex_selector *sel);

/* === Dump === */
void css_selector_dump(css_selector_list *list, FILE *out, int depth);

#endif /* CSS_SELECTOR_H */
```

**Step 2: Verify it compiles**

Run: `cc -std=c11 -Wall -Wextra -pedantic -fsyntax-only -Iinclude include/css_selector.h`
Expected: no errors

**Step 3: Commit**

```bash
git add include/css_selector.h
git commit -m "feat: selector data structure definitions (css_selector.h)"
```

---

### Task 2: Selector lifecycle functions (create/free/append)

**Files:**
- Create: `src/css_selector.c`
- Modify: `Makefile:4` — add `src/css_selector.c` to SRC

**Step 1: Write css_selector.c with lifecycle functions**

```c
#define _POSIX_C_SOURCE 200809L

#include "css_selector.h"
#include <stdlib.h>
#include <string.h>

/* === Simple selector === */

css_simple_selector *css_simple_selector_create(css_simple_selector_type type)
{
    css_simple_selector *sel = calloc(1, sizeof(css_simple_selector));
    if (!sel) return NULL;
    sel->type = type;
    return sel;
}

void css_simple_selector_free(css_simple_selector *sel)
{
    if (!sel) return;
    free(sel->name);
    free(sel->attr_name);
    free(sel->attr_value);
    free(sel);
}

/* === Compound selector === */

css_compound_selector *css_compound_selector_create(void)
{
    return calloc(1, sizeof(css_compound_selector));
}

void css_compound_selector_free(css_compound_selector *sel)
{
    if (!sel) return;
    for (size_t i = 0; i < sel->count; i++)
        css_simple_selector_free(sel->selectors[i]);
    free(sel->selectors);
    free(sel);
}

void css_compound_selector_append(css_compound_selector *comp,
                                   css_simple_selector *simple)
{
    if (!comp || !simple) return;
    if (comp->count >= comp->cap) {
        comp->cap = comp->cap ? comp->cap * 2 : 4;
        comp->selectors = realloc(comp->selectors,
                                   comp->cap * sizeof(css_simple_selector *));
    }
    comp->selectors[comp->count++] = simple;
}

/* === Complex selector === */

css_complex_selector *css_complex_selector_create(void)
{
    return calloc(1, sizeof(css_complex_selector));
}

void css_complex_selector_free(css_complex_selector *sel)
{
    if (!sel) return;
    for (size_t i = 0; i < sel->count; i++)
        css_compound_selector_free(sel->compounds[i]);
    free(sel->compounds);
    free(sel->combinators);
    free(sel);
}

void css_complex_selector_append(css_complex_selector *complex,
                                  css_compound_selector *compound,
                                  css_combinator comb)
{
    if (!complex || !compound) return;
    if (complex->count >= complex->cap) {
        complex->cap = complex->cap ? complex->cap * 2 : 4;
        complex->compounds = realloc(complex->compounds,
                                      complex->cap * sizeof(css_compound_selector *));
        complex->combinators = realloc(complex->combinators,
                                        complex->cap * sizeof(css_combinator));
    }
    if (complex->count > 0)
        complex->combinators[complex->count - 1] = comb;
    complex->compounds[complex->count++] = compound;
}

/* === Selector list === */

css_selector_list *css_selector_list_create(void)
{
    return calloc(1, sizeof(css_selector_list));
}

void css_selector_list_free(css_selector_list *list)
{
    if (!list) return;
    for (size_t i = 0; i < list->count; i++)
        css_complex_selector_free(list->selectors[i]);
    free(list->selectors);
    free(list);
}

void css_selector_list_append(css_selector_list *list,
                               css_complex_selector *complex)
{
    if (!list || !complex) return;
    if (list->count >= list->cap) {
        list->cap = list->cap ? list->cap * 2 : 4;
        list->selectors = realloc(list->selectors,
                                   list->cap * sizeof(css_complex_selector *));
    }
    list->selectors[list->count++] = complex;
}
```

**Step 2: Update Makefile — add css_selector.c to SRC**

Change line 4:
```makefile
SRC = src/css_token.c src/css_tokenizer.c src/css_ast.c src/css_parser.c src/css_selector.c
```

**Step 3: Verify build**

Run: `make clean && make css_parse`
Expected: compiles with zero warnings (unused function warnings acceptable temporarily)

**Step 4: Commit**

```bash
git add src/css_selector.c Makefile
git commit -m "feat: selector lifecycle functions (create/free/append)"
```

---

### Task 3: Selector dump function

**Files:**
- Modify: `src/css_selector.c` — add `css_selector_dump()`

**Step 1: Add dump function to css_selector.c**

```c
/* === Dump === */

static const char *simple_sel_type_name(css_simple_selector_type type)
{
    switch (type) {
    case SEL_TYPE:           return "type";
    case SEL_UNIVERSAL:      return "universal";
    case SEL_CLASS:          return "class";
    case SEL_ID:             return "id";
    case SEL_ATTRIBUTE:      return "attribute";
    case SEL_PSEUDO_CLASS:   return "pseudo-class";
    case SEL_PSEUDO_ELEMENT: return "pseudo-element";
    default:                 return "unknown";
    }
}

static const char *combinator_name(css_combinator comb)
{
    switch (comb) {
    case COMB_DESCENDANT:          return " ";
    case COMB_CHILD:               return ">";
    case COMB_NEXT_SIBLING:        return "+";
    case COMB_SUBSEQUENT_SIBLING:  return "~";
    default:                       return "?";
    }
}

static const char *attr_match_name(css_attr_match match)
{
    switch (match) {
    case ATTR_EXISTS:    return "";
    case ATTR_EXACT:     return "=";
    case ATTR_INCLUDES:  return "~=";
    case ATTR_DASH_MATCH:return "|=";
    case ATTR_PREFIX:    return "^=";
    case ATTR_SUFFIX:    return "$=";
    case ATTR_SUBSTRING: return "*=";
    default:             return "?=";
    }
}

static void dump_indent_s(FILE *out, int depth)
{
    for (int i = 0; i < depth; i++) fprintf(out, "  ");
}

void css_selector_dump(css_selector_list *list, FILE *out, int depth)
{
    if (!list || !out) return;
    dump_indent_s(out, depth);
    fprintf(out, "SELECTOR_LIST (%zu)\n", list->count);
    for (size_t i = 0; i < list->count; i++) {
        css_complex_selector *cs = list->selectors[i];
        if (!cs) continue;
        dump_indent_s(out, depth + 1);
        fprintf(out, "COMPLEX_SELECTOR\n");
        for (size_t j = 0; j < cs->count; j++) {
            if (j > 0) {
                dump_indent_s(out, depth + 2);
                fprintf(out, "COMBINATOR \"%s\"\n",
                        combinator_name(cs->combinators[j - 1]));
            }
            css_compound_selector *comp = cs->compounds[j];
            if (!comp) continue;
            dump_indent_s(out, depth + 2);
            fprintf(out, "COMPOUND_SELECTOR\n");
            for (size_t k = 0; k < comp->count; k++) {
                css_simple_selector *ss = comp->selectors[k];
                if (!ss) continue;
                dump_indent_s(out, depth + 3);
                if (ss->type == SEL_ATTRIBUTE) {
                    fprintf(out, "<%s [%s%s%s%s]>\n",
                            simple_sel_type_name(ss->type),
                            ss->attr_name ? ss->attr_name : "",
                            attr_match_name(ss->attr_match),
                            ss->attr_value ? "\"" : "",
                            ss->attr_value ? ss->attr_value : "");
                    if (ss->attr_value) {
                        /* close the quote inline — handled by format above */
                    }
                } else {
                    fprintf(out, "<%s \"%s\">\n",
                            simple_sel_type_name(ss->type),
                            ss->name ? ss->name : "*");
                }
            }
        }
    }
}
```

**Step 2: Verify build**

Run: `make clean && make css_parse`
Expected: zero warnings

**Step 3: Commit**

```bash
git add src/css_selector.c
git commit -m "feat: selector dump function for debug output"
```

---

### Task 4: Core selector parsing — compound selectors

**Files:**
- Modify: `src/css_selector.c` — add parsing helpers and `parse_compound_selector()`

**Step 1: Add CV helper functions and compound parsing**

Helper functions needed at top of parsing section in `css_selector.c`:

```c
/* === Parsing helpers === */

/* Check if a component value is a preserved token of specific type */
static bool cv_is(css_component_value *cv, css_token_type type)
{
    return cv && cv->type == CSS_NODE_COMPONENT_VALUE
           && cv->u.token && cv->u.token->type == type;
}

/* Check if a component value is a delim with specific codepoint */
static bool cv_is_delim(css_component_value *cv, uint32_t cp)
{
    return cv && cv->type == CSS_NODE_COMPONENT_VALUE
           && cv->u.token && cv->u.token->type == CSS_TOKEN_DELIM
           && cv->u.token->delim_codepoint == cp;
}

/* Get token value string from a CV (NULL if not a token or no value) */
static const char *cv_token_value(css_component_value *cv)
{
    if (!cv || cv->type != CSS_NODE_COMPONENT_VALUE || !cv->u.token)
        return NULL;
    return cv->u.token->value;
}

/* Parse attribute selector from a [...] simple block */
static css_simple_selector *parse_attribute_selector(css_simple_block *block)
{
    if (!block || block->associated_token != CSS_TOKEN_OPEN_SQUARE)
        return NULL;

    css_component_value **v = block->values;
    size_t n = block->value_count;
    size_t pos = 0;

    /* skip leading whitespace */
    while (pos < n && cv_is(v[pos], CSS_TOKEN_WHITESPACE)) pos++;

    /* attribute name (ident) */
    if (pos >= n || !cv_is(v[pos], CSS_TOKEN_IDENT)) return NULL;
    const char *attr_name = cv_token_value(v[pos]);
    pos++;

    /* skip whitespace */
    while (pos < n && cv_is(v[pos], CSS_TOKEN_WHITESPACE)) pos++;

    /* if no more tokens → [attr] exists check */
    if (pos >= n) {
        css_simple_selector *sel = css_simple_selector_create(SEL_ATTRIBUTE);
        if (!sel) return NULL;
        sel->attr_match = ATTR_EXISTS;
        sel->attr_name = strdup(attr_name);
        return sel;
    }

    /* match operator */
    css_attr_match match = ATTR_EXISTS;
    if (cv_is_delim(v[pos], '=')) {
        match = ATTR_EXACT;
        pos++;
    } else if (cv_is_delim(v[pos], '~')) {
        pos++;
        if (pos < n && cv_is_delim(v[pos], '=')) { match = ATTR_INCLUDES; pos++; }
        else return NULL;
    } else if (cv_is_delim(v[pos], '|')) {
        pos++;
        if (pos < n && cv_is_delim(v[pos], '=')) { match = ATTR_DASH_MATCH; pos++; }
        else return NULL;
    } else if (cv_is_delim(v[pos], '^')) {
        pos++;
        if (pos < n && cv_is_delim(v[pos], '=')) { match = ATTR_PREFIX; pos++; }
        else return NULL;
    } else if (cv_is_delim(v[pos], '$')) {
        pos++;
        if (pos < n && cv_is_delim(v[pos], '=')) { match = ATTR_SUFFIX; pos++; }
        else return NULL;
    } else if (cv_is_delim(v[pos], '*')) {
        pos++;
        if (pos < n && cv_is_delim(v[pos], '=')) { match = ATTR_SUBSTRING; pos++; }
        else return NULL;
    } else {
        return NULL; /* unknown operator */
    }

    /* skip whitespace */
    while (pos < n && cv_is(v[pos], CSS_TOKEN_WHITESPACE)) pos++;

    /* attribute value (ident or string) */
    if (pos >= n) return NULL;
    const char *attr_value = NULL;
    if (cv_is(v[pos], CSS_TOKEN_IDENT) || cv_is(v[pos], CSS_TOKEN_STRING)) {
        attr_value = cv_token_value(v[pos]);
        pos++;
    } else {
        return NULL;
    }

    /* skip whitespace */
    while (pos < n && cv_is(v[pos], CSS_TOKEN_WHITESPACE)) pos++;

    /* optional case flag: i or s */
    bool case_insensitive = false;
    if (pos < n && cv_is(v[pos], CSS_TOKEN_IDENT)) {
        const char *flag = cv_token_value(v[pos]);
        if (flag && (flag[0] == 'i' || flag[0] == 'I') && flag[1] == '\0') {
            case_insensitive = true;
            pos++;
        } else if (flag && (flag[0] == 's' || flag[0] == 'S') && flag[1] == '\0') {
            pos++; /* explicit case-sensitive, default behavior */
        }
    }

    css_simple_selector *sel = css_simple_selector_create(SEL_ATTRIBUTE);
    if (!sel) return NULL;
    sel->attr_match = match;
    sel->attr_name = strdup(attr_name);
    if (attr_value) sel->attr_value = strdup(attr_value);
    sel->attr_case_insensitive = case_insensitive;
    return sel;
}

/* Parse a compound selector from cv[*pos] onward.
 * Advances *pos past consumed tokens.
 * Returns NULL if no valid compound selector found. */
static css_compound_selector *parse_compound_selector(
    css_component_value **values, size_t count, size_t *pos)
{
    css_compound_selector *comp = css_compound_selector_create();
    if (!comp) return NULL;

    size_t p = *pos;

    /* 1. Optional type selector or universal selector */
    if (p < count) {
        if (cv_is(values[p], CSS_TOKEN_IDENT)) {
            /* type selector: div, p, span, etc. */
            css_simple_selector *sel = css_simple_selector_create(SEL_TYPE);
            if (sel) {
                sel->name = strdup(cv_token_value(values[p]));
                css_compound_selector_append(comp, sel);
            }
            p++;
        } else if (cv_is_delim(values[p], '*')) {
            /* universal selector */
            css_simple_selector *sel = css_simple_selector_create(SEL_UNIVERSAL);
            if (sel) css_compound_selector_append(comp, sel);
            p++;
        }
    }

    /* 2. Subclass selectors loop */
    while (p < count) {
        if (cv_is(values[p], CSS_TOKEN_HASH)) {
            /* ID selector: #id */
            css_simple_selector *sel = css_simple_selector_create(SEL_ID);
            if (sel) {
                sel->name = strdup(cv_token_value(values[p]));
                css_compound_selector_append(comp, sel);
            }
            p++;
        } else if (cv_is_delim(values[p], '.') && p + 1 < count
                   && cv_is(values[p + 1], CSS_TOKEN_IDENT)) {
            /* Class selector: .classname */
            css_simple_selector *sel = css_simple_selector_create(SEL_CLASS);
            if (sel) {
                sel->name = strdup(cv_token_value(values[p + 1]));
                css_compound_selector_append(comp, sel);
            }
            p += 2;
        } else if (values[p]->type == CSS_NODE_SIMPLE_BLOCK
                   && values[p]->u.block
                   && values[p]->u.block->associated_token == CSS_TOKEN_OPEN_SQUARE) {
            /* Attribute selector: [attr...] */
            css_simple_selector *sel = parse_attribute_selector(values[p]->u.block);
            if (sel) {
                css_compound_selector_append(comp, sel);
            }
            p++;
        } else if (cv_is(values[p], CSS_TOKEN_COLON)) {
            /* Pseudo-class or pseudo-element */
            if (p + 1 < count && cv_is(values[p + 1], CSS_TOKEN_COLON)) {
                /* :: pseudo-element */
                if (p + 2 < count && cv_is(values[p + 2], CSS_TOKEN_IDENT)) {
                    css_simple_selector *sel = css_simple_selector_create(SEL_PSEUDO_ELEMENT);
                    if (sel) {
                        sel->name = strdup(cv_token_value(values[p + 2]));
                        css_compound_selector_append(comp, sel);
                    }
                    p += 3;
                } else {
                    break; /* invalid */
                }
            } else if (p + 1 < count && cv_is(values[p + 1], CSS_TOKEN_IDENT)) {
                /* :pseudo-class */
                css_simple_selector *sel = css_simple_selector_create(SEL_PSEUDO_CLASS);
                if (sel) {
                    sel->name = strdup(cv_token_value(values[p + 1]));
                    css_compound_selector_append(comp, sel);
                }
                p += 2;
            } else {
                break; /* unrecognized */
            }
        } else {
            break; /* not a subclass selector */
        }
    }

    *pos = p;

    /* Must have at least one simple selector */
    if (comp->count == 0) {
        css_compound_selector_free(comp);
        return NULL;
    }
    return comp;
}
```

**Step 2: Verify build**

Run: `make clean && make css_parse`
Expected: zero warnings (parse_compound_selector may warn unused — acceptable temporarily)

**Step 3: Commit**

```bash
git add src/css_selector.c
git commit -m "feat: compound selector parsing with attribute support"
```

---

### Task 5: Complex selector and selector list parsing

**Files:**
- Modify: `src/css_selector.c` — add `parse_complex_selector()` and `css_parse_selector_list()`

**Step 1: Add complex selector parsing**

```c
/* Parse a complex selector from cv[start..end).
 * Returns NULL on parse error. */
static css_complex_selector *parse_complex_selector(
    css_component_value **values, size_t start, size_t end)
{
    css_complex_selector *cs = css_complex_selector_create();
    if (!cs) return NULL;

    size_t pos = start;

    /* skip leading whitespace */
    while (pos < end && cv_is(values[pos], CSS_TOKEN_WHITESPACE)) pos++;

    /* parse first compound selector */
    css_compound_selector *first = parse_compound_selector(values, end, &pos);
    if (!first) {
        css_complex_selector_free(cs);
        return NULL;
    }
    css_complex_selector_append(cs, first, COMB_DESCENDANT); /* comb unused for first */

    /* parse [combinator compound]* */
    while (pos < end) {
        /* determine combinator */
        css_combinator comb = COMB_DESCENDANT;
        bool found_ws = false;

        /* skip whitespace */
        while (pos < end && cv_is(values[pos], CSS_TOKEN_WHITESPACE)) {
            found_ws = true;
            pos++;
        }

        if (pos >= end) break;

        /* explicit combinator? */
        if (cv_is_delim(values[pos], '>')) {
            comb = COMB_CHILD;
            pos++;
        } else if (cv_is_delim(values[pos], '+')) {
            comb = COMB_NEXT_SIBLING;
            pos++;
        } else if (cv_is_delim(values[pos], '~')) {
            comb = COMB_SUBSEQUENT_SIBLING;
            pos++;
        } else if (found_ws) {
            comb = COMB_DESCENDANT;
        } else {
            break; /* no combinator found */
        }

        /* skip whitespace after combinator */
        while (pos < end && cv_is(values[pos], CSS_TOKEN_WHITESPACE)) pos++;

        if (pos >= end) {
            /* trailing combinator with no compound → error */
            css_complex_selector_free(cs);
            return NULL;
        }

        css_compound_selector *comp = parse_compound_selector(values, end, &pos);
        if (!comp) {
            css_complex_selector_free(cs);
            return NULL;
        }
        css_complex_selector_append(cs, comp, comb);
    }

    return cs;
}
```

**Step 2: Add selector list parsing (public API)**

```c
css_selector_list *css_parse_selector_list(css_component_value **values,
                                            size_t count)
{
    if (!values || count == 0) return NULL;

    css_selector_list *list = css_selector_list_create();
    if (!list) return NULL;

    /* split by comma tokens at the top level */
    size_t start = 0;
    for (size_t i = 0; i <= count; i++) {
        bool is_comma = (i < count && cv_is(values[i], CSS_TOKEN_COMMA));
        bool is_end = (i == count);

        if (is_comma || is_end) {
            /* parse complex selector from values[start..i) */
            if (i > start) {
                css_complex_selector *cs =
                    parse_complex_selector(values, start, i);
                if (!cs) {
                    /* one invalid → entire list invalid */
                    css_selector_list_free(list);
                    return NULL;
                }
                css_selector_list_append(list, cs);
            }
            start = i + 1;
        }
    }

    if (list->count == 0) {
        css_selector_list_free(list);
        return NULL;
    }

    return list;
}
```

**Step 3: Verify build**

Run: `make clean && make css_parse`
Expected: zero warnings

**Step 4: Commit**

```bash
git add src/css_selector.c
git commit -m "feat: complex selector and selector list parsing"
```

---

### Task 6: Specificity calculation

**Files:**
- Modify: `src/css_selector.c` — add `css_selector_specificity()`

**Step 1: Implement specificity**

```c
css_specificity css_selector_specificity(css_complex_selector *sel)
{
    css_specificity spec = {0, 0, 0};
    if (!sel) return spec;

    for (size_t i = 0; i < sel->count; i++) {
        css_compound_selector *comp = sel->compounds[i];
        if (!comp) continue;
        for (size_t j = 0; j < comp->count; j++) {
            css_simple_selector *ss = comp->selectors[j];
            if (!ss) continue;
            switch (ss->type) {
            case SEL_ID:             spec.a++; break;
            case SEL_CLASS:          spec.b++; break;
            case SEL_ATTRIBUTE:      spec.b++; break;
            case SEL_PSEUDO_CLASS:   spec.b++; break;
            case SEL_TYPE:           spec.c++; break;
            case SEL_PSEUDO_ELEMENT: spec.c++; break;
            case SEL_UNIVERSAL:      break; /* no specificity */
            }
        }
    }
    return spec;
}
```

**Step 2: Verify build**

Run: `make clean && make css_parse`
Expected: zero warnings

**Step 3: Commit**

```bash
git add src/css_selector.c
git commit -m "feat: selector specificity calculation"
```

---

### Task 7: Integrate into AST and parser

**Files:**
- Modify: `include/css_ast.h:74-80` — add `selectors` field to `css_qualified_rule`
- Modify: `src/css_ast.c:167-176` — free selectors in `css_qualified_rule_free()`
- Modify: `src/css_parser.c:500-531` — call selector parsing after building stylesheet
- Modify: `src/css_parser.c:689+` — update dump to show selectors

**Step 1: Add selectors field to css_qualified_rule in css_ast.h**

In `include/css_ast.h`, add forward declaration and field:

```c
/* Forward declaration for selector (defined in css_selector.h) */
typedef struct css_selector_list css_selector_list;
```
(Add before the existing forward declarations block, line ~20)

In `struct css_qualified_rule` (line 75-80), add after `block`:
```c
    css_selector_list *selectors;  /* parsed selector list (may be NULL) */
```

**Step 2: Update css_qualified_rule_free in css_ast.c**

Add `#include "css_selector.h"` at top (after existing includes).

In `css_qualified_rule_free()` (line 167-176), add before `free(qr)`:
```c
    css_selector_list_free(qr->selectors);
```

**Step 3: Update css_parse_stylesheet in css_parser.c**

Add `#include "css_selector.h"` at top.

After line 514 (`consume_list_of_rules(&parser, sheet, true);`), add selector parsing:

```c
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
```

**Step 4: Update dump in css_parser.c**

In `css_parse_dump()`, find where QUALIFIED_RULE is dumped (around line 717). After printing "QUALIFIED_RULE\n", add selector dump before the prelude dump:

```c
            if (qr->selectors) {
                css_selector_dump(qr->selectors, out, depth + 1);
            }
```

**Step 5: Verify build**

Run: `make clean && make css_parse`
Expected: zero warnings

**Step 6: Commit**

```bash
git add include/css_ast.h src/css_ast.c src/css_parser.c
git commit -m "feat: integrate selector parsing into parser pipeline"
```

---

### Task 8: Test file and end-to-end verification

**Files:**
- Create: `tests/selectors.css`
- Modify: `Makefile` — add selector test target

**Step 1: Create comprehensive test file**

```css
/* Basic type selectors */
body { margin: 0; }
div { display: block; }

/* Universal selector */
* { box-sizing: border-box; }

/* Class selectors */
.container { width: 100%; }
.btn.primary { color: white; }

/* ID selectors */
#header { height: 60px; }

/* Compound selectors */
div.container#main { padding: 10px; }

/* Descendant combinator */
div p { color: black; }

/* Child combinator */
ul > li { list-style: none; }

/* Next-sibling combinator */
h1 + p { margin-top: 0; }

/* Subsequent-sibling combinator */
h1 ~ p { color: gray; }

/* Attribute selectors */
[href] { color: blue; }
[type="text"] { border: 1px solid; }
[class~="active"] { font-weight: bold; }
[lang|="en"] { color: green; }
[href^="https"] { text-decoration: none; }
[href$=".pdf"] { color: red; }
[data-value*="test"] { background: yellow; }
[type="text" i] { outline: none; }

/* Pseudo-classes */
a:hover { color: red; }
li:first-child { font-weight: bold; }
input:focus { border-color: blue; }

/* Pseudo-elements */
p::before { content: ""; }
p::after { content: ""; }

/* Complex selectors */
.nav > ul > li > a:hover { color: red; }

/* Selector list (comma-separated) */
h1, h2, h3 { font-weight: bold; }
.btn, .link, a:hover { cursor: pointer; }
```

**Step 2: Update Makefile**

Add after `test-errors` target:
```makefile
test-selectors: css_parse
	./css_parse tests/selectors.css
```

Update `test-all`:
```makefile
test-all: test test-tokens test-errors test-selectors
```

**Step 3: Run test**

Run: `make clean && make test-selectors`
Expected: AST output with SELECTOR_LIST nodes showing parsed selector structure for each qualified rule.

**Step 4: Run full test suite**

Run: `make test-all`
Expected: all tests pass

**Step 5: Run AddressSanitizer**

Run:
```bash
cc -std=c11 -Wall -Wextra -pedantic -g -fsanitize=address,undefined \
   -Iinclude src/css_token.c src/css_tokenizer.c src/css_ast.c \
   src/css_parser.c src/css_selector.c src/css_parse_demo.c -o css_parse_asan
./css_parse_asan tests/selectors.css
./css_parse_asan tests/basic.css
./css_parse_asan tests/declarations.css
./css_parse_asan tests/at_rules.css
```
Expected: zero memory errors

**Step 6: Commit**

```bash
git add tests/selectors.css Makefile
git commit -m "feat: selector parsing tests and ASAN verification"
```

---

### Task 9: Update documentation (know.md, list.md, CLAUDE.md)

**Files:**
- Modify: `know.md` — add P2a selector knowledge
- Modify: `list.md` — add P2a task tracking
- Modify: `CLAUDE.md` — update implementation status

**Step 1: Update know.md**

Add section for P2a covering:
- Selector hierarchy (simple → compound → complex → list)
- Attribute selector parsing (7 match types)
- Combinator detection (whitespace = descendant)
- Specificity calculation rules
- Integration point (qualified_rule.selectors)

**Step 2: Update list.md**

Add P2a tasks (1-9) with completion status.

**Step 3: Update CLAUDE.md**

- Add `src/css_selector.c` and `include/css_selector.h` to implemented files
- Update P2 status from "Not started" to "P2a complete"

**Step 4: Commit**

```bash
git add know.md list.md CLAUDE.md
git commit -m "docs: update documentation for P2a selector parser"
```
