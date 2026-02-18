#define _POSIX_C_SOURCE 200809L

#include "css_selector.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Simple selector lifecycle
 * ================================================================ */

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

/* ================================================================
 * Compound selector lifecycle
 * ================================================================ */

css_compound_selector *css_compound_selector_create(void)
{
    css_compound_selector *comp = calloc(1, sizeof(css_compound_selector));
    return comp;
}

void css_compound_selector_free(css_compound_selector *comp)
{
    if (!comp) return;
    for (size_t i = 0; i < comp->count; i++) {
        css_simple_selector_free(comp->selectors[i]);
    }
    free(comp->selectors);
    free(comp);
}

void css_compound_selector_append(css_compound_selector *comp,
                                  css_simple_selector *sel)
{
    if (!comp || !sel) return;
    if (comp->count >= comp->cap) {
        comp->cap = comp->cap ? comp->cap * 2 : 4;
        comp->selectors = realloc(comp->selectors,
                                  comp->cap * sizeof(css_simple_selector *));
    }
    comp->selectors[comp->count++] = sel;
}

/* ================================================================
 * Complex selector lifecycle
 * ================================================================ */

css_complex_selector *css_complex_selector_create(void)
{
    css_complex_selector *cx = calloc(1, sizeof(css_complex_selector));
    return cx;
}

void css_complex_selector_free(css_complex_selector *cx)
{
    if (!cx) return;
    for (size_t i = 0; i < cx->count; i++) {
        css_compound_selector_free(cx->compounds[i]);
    }
    free(cx->compounds);
    free(cx->combinators);
    free(cx);
}

void css_complex_selector_append(css_complex_selector *cx,
                                 css_compound_selector *comp,
                                 css_combinator comb)
{
    if (!cx || !comp) return;
    if (cx->count >= cx->cap) {
        cx->cap = cx->cap ? cx->cap * 2 : 4;
        cx->compounds = realloc(cx->compounds,
                                cx->cap * sizeof(css_compound_selector *));
        /* combinators array: at most (cap - 1) entries, but allocate cap
         * for simplicity — the extra slot is never read */
        cx->combinators = realloc(cx->combinators,
                                  cx->cap * sizeof(css_combinator));
    }
    /* If this is not the first compound, store the combinator that sits
     * between the previous compound and this one. */
    if (cx->count > 0) {
        cx->combinators[cx->count - 1] = comb;
    }
    cx->compounds[cx->count++] = comp;
}

/* ================================================================
 * Selector list lifecycle
 * ================================================================ */

css_selector_list *css_selector_list_create(void)
{
    css_selector_list *list = calloc(1, sizeof(css_selector_list));
    return list;
}

void css_selector_list_free(css_selector_list *list)
{
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        css_complex_selector_free(list->selectors[i]);
    }
    free(list->selectors);
    free(list);
}

void css_selector_list_append(css_selector_list *list,
                              css_complex_selector *cx)
{
    if (!list || !cx) return;
    if (list->count >= list->cap) {
        list->cap = list->cap ? list->cap * 2 : 4;
        list->selectors = realloc(list->selectors,
                                  list->cap * sizeof(css_complex_selector *));
    }
    list->selectors[list->count++] = cx;
}

/* ================================================================
 * Dump helpers (static)
 * ================================================================ */

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
    }
    return "unknown";
}

static const char *combinator_name(css_combinator comb)
{
    switch (comb) {
    case COMB_DESCENDANT:          return " ";
    case COMB_CHILD:               return ">";
    case COMB_NEXT_SIBLING:        return "+";
    case COMB_SUBSEQUENT_SIBLING:  return "~";
    }
    return "?";
}

static const char *attr_match_name(css_attr_match match)
{
    switch (match) {
    case ATTR_EXISTS:    return "";
    case ATTR_EXACT:     return "=";
    case ATTR_INCLUDES:  return "~=";
    case ATTR_DASH:      return "|=";
    case ATTR_PREFIX:    return "^=";
    case ATTR_SUFFIX:    return "$=";
    case ATTR_SUBSTRING: return "*=";
    }
    return "?";
}

static void dump_indent_s(FILE *out, int depth)
{
    for (int i = 0; i < depth; i++) {
        fprintf(out, "  ");
    }
}

/* ================================================================
 * Dump (public)
 * ================================================================ */

void css_selector_dump(css_selector_list *list, FILE *out, int depth)
{
    if (!list || !out) return;

    dump_indent_s(out, depth);
    fprintf(out, "SELECTOR_LIST (%zu)\n", list->count);

    for (size_t i = 0; i < list->count; i++) {
        css_complex_selector *cx = list->selectors[i];
        if (!cx) continue;

        dump_indent_s(out, depth + 1);
        fprintf(out, "COMPLEX_SELECTOR\n");

        for (size_t j = 0; j < cx->count; j++) {
            /* Print combinator before compound (except for the first) */
            if (j > 0) {
                dump_indent_s(out, depth + 2);
                fprintf(out, "COMBINATOR \"%s\"\n",
                        combinator_name(cx->combinators[j - 1]));
            }

            css_compound_selector *comp = cx->compounds[j];
            if (!comp) continue;

            dump_indent_s(out, depth + 2);
            fprintf(out, "COMPOUND_SELECTOR\n");

            for (size_t k = 0; k < comp->count; k++) {
                css_simple_selector *sel = comp->selectors[k];
                if (!sel) continue;

                dump_indent_s(out, depth + 3);

                if (sel->type == SEL_ATTRIBUTE) {
                    /* <attribute [href^="https"]> */
                    fprintf(out, "<%s [%s",
                            simple_sel_type_name(sel->type),
                            sel->attr_name ? sel->attr_name : "");
                    if (sel->attr_match != ATTR_EXISTS && sel->attr_value) {
                        fprintf(out, "%s\"%s\"",
                                attr_match_name(sel->attr_match),
                                sel->attr_value);
                    }
                    if (sel->attr_case_insensitive) {
                        fprintf(out, " i");
                    }
                    fprintf(out, "]>\n");
                } else {
                    /* <type "div">, <class "foo">, <id "bar">, <universal>, etc. */
                    if (sel->name) {
                        fprintf(out, "<%s \"%s\">\n",
                                simple_sel_type_name(sel->type),
                                sel->name);
                    } else {
                        fprintf(out, "<%s>\n",
                                simple_sel_type_name(sel->type));
                    }
                }
            }
        }
    }
}

/* ================================================================
 * Stubs — implemented in later tasks
 * ================================================================ */

/* Stub -- implemented in Task 4-5 */
css_selector_list *css_parse_selector_list(css_component_value **values,
                                           size_t count)
{
    (void)values; (void)count;
    return NULL;
}

/* Stub -- implemented in Task 6 */
css_specificity css_selector_specificity(css_complex_selector *sel)
{
    css_specificity spec = {0, 0, 0};
    (void)sel;
    return spec;
}
