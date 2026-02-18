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
 * CV helper functions (Task 4)
 * ================================================================ */

/* Check if a component value is a preserved token of specific type */
static bool cv_is(css_component_value *cv, css_token_type type)
{
    if (!cv) return false;
    return cv->type == CSS_NODE_COMPONENT_VALUE && cv->u.token &&
           cv->u.token->type == type;
}

/* Check if a component value is a delim with specific codepoint */
static bool cv_is_delim(css_component_value *cv, uint32_t cp)
{
    if (!cv) return false;
    return cv->type == CSS_NODE_COMPONENT_VALUE && cv->u.token &&
           cv->u.token->type == CSS_TOKEN_DELIM &&
           cv->u.token->delim_codepoint == cp;
}

/* Get token value string from a CV (NULL if not applicable) */
static const char *cv_token_value(css_component_value *cv)
{
    if (!cv || cv->type != CSS_NODE_COMPONENT_VALUE || !cv->u.token)
        return NULL;
    return cv->u.token->value;
}

/* ================================================================
 * Attribute selector parsing (Task 4)
 * ================================================================ */

static css_simple_selector *parse_attribute_selector(css_simple_block *block)
{
    if (!block || block->associated_token != CSS_TOKEN_OPEN_SQUARE)
        return NULL;

    css_component_value **vals = block->values;
    size_t cnt = block->value_count;
    size_t pos = 0;

    /* Skip leading whitespace */
    while (pos < cnt && cv_is(vals[pos], CSS_TOKEN_WHITESPACE))
        pos++;

    /* Read attr_name (must be ident) */
    if (pos >= cnt || !cv_is(vals[pos], CSS_TOKEN_IDENT))
        return NULL;

    const char *attr_name = cv_token_value(vals[pos]);
    if (!attr_name) return NULL;
    pos++;

    /* Skip whitespace after attr_name */
    while (pos < cnt && cv_is(vals[pos], CSS_TOKEN_WHITESPACE))
        pos++;

    /* If no more tokens -> ATTR_EXISTS */
    if (pos >= cnt) {
        css_simple_selector *sel = css_simple_selector_create(SEL_ATTRIBUTE);
        if (!sel) return NULL;
        sel->attr_name = strdup(attr_name);
        sel->attr_match = ATTR_EXISTS;
        return sel;
    }

    /* Determine match operator */
    css_attr_match match = ATTR_EXISTS;

    if (cv_is_delim(vals[pos], '=')) {
        /* Exact match: = */
        match = ATTR_EXACT;
        pos++;
    } else if (cv_is_delim(vals[pos], '~') &&
               pos + 1 < cnt && cv_is_delim(vals[pos + 1], '=')) {
        match = ATTR_INCLUDES;
        pos += 2;
    } else if (cv_is_delim(vals[pos], '|') &&
               pos + 1 < cnt && cv_is_delim(vals[pos + 1], '=')) {
        match = ATTR_DASH;
        pos += 2;
    } else if (cv_is_delim(vals[pos], '^') &&
               pos + 1 < cnt && cv_is_delim(vals[pos + 1], '=')) {
        match = ATTR_PREFIX;
        pos += 2;
    } else if (cv_is_delim(vals[pos], '$') &&
               pos + 1 < cnt && cv_is_delim(vals[pos + 1], '=')) {
        match = ATTR_SUFFIX;
        pos += 2;
    } else if (cv_is_delim(vals[pos], '*') &&
               pos + 1 < cnt && cv_is_delim(vals[pos + 1], '=')) {
        match = ATTR_SUBSTRING;
        pos += 2;
    } else {
        /* No operator found after attr_name -> ATTR_EXISTS */
        css_simple_selector *sel = css_simple_selector_create(SEL_ATTRIBUTE);
        if (!sel) return NULL;
        sel->attr_name = strdup(attr_name);
        sel->attr_match = ATTR_EXISTS;
        return sel;
    }

    /* Skip whitespace after operator */
    while (pos < cnt && cv_is(vals[pos], CSS_TOKEN_WHITESPACE))
        pos++;

    /* Read attr_value (ident or string) */
    if (pos >= cnt) return NULL;

    const char *attr_value = NULL;
    if (cv_is(vals[pos], CSS_TOKEN_IDENT) || cv_is(vals[pos], CSS_TOKEN_STRING)) {
        attr_value = cv_token_value(vals[pos]);
        pos++;
    } else {
        return NULL; /* invalid attribute value */
    }

    /* Skip whitespace */
    while (pos < cnt && cv_is(vals[pos], CSS_TOKEN_WHITESPACE))
        pos++;

    /* Optional case flag: i or s */
    bool case_insensitive = false;
    if (pos < cnt && cv_is(vals[pos], CSS_TOKEN_IDENT)) {
        const char *flag = cv_token_value(vals[pos]);
        if (flag && (flag[0] == 'i' || flag[0] == 'I') && flag[1] == '\0') {
            case_insensitive = true;
            pos++;
        } else if (flag && (flag[0] == 's' || flag[0] == 'S') && flag[1] == '\0') {
            /* explicit case-sensitive, default behavior */
            pos++;
        }
    }

    css_simple_selector *sel = css_simple_selector_create(SEL_ATTRIBUTE);
    if (!sel) return NULL;
    sel->attr_name = strdup(attr_name);
    sel->attr_match = match;
    sel->attr_value = attr_value ? strdup(attr_value) : NULL;
    sel->attr_case_insensitive = case_insensitive;
    return sel;
}

/* ================================================================
 * Compound selector parsing (Task 4)
 * ================================================================ */

static css_compound_selector *parse_compound_selector(
    css_component_value **values, size_t count, size_t *pos)
{
    if (!values || *pos >= count) return NULL;

    css_compound_selector *comp = css_compound_selector_create();
    if (!comp) return NULL;

    size_t p = *pos;

    /* 1. Try type selector: ident -> SEL_TYPE, delim('*') -> SEL_UNIVERSAL */
    if (p < count && cv_is(values[p], CSS_TOKEN_IDENT)) {
        const char *name = cv_token_value(values[p]);
        if (name) {
            css_simple_selector *sel = css_simple_selector_create(SEL_TYPE);
            if (sel) {
                sel->name = strdup(name);
                css_compound_selector_append(comp, sel);
            }
            p++;
        }
    } else if (p < count && cv_is_delim(values[p], '*')) {
        css_simple_selector *sel = css_simple_selector_create(SEL_UNIVERSAL);
        if (sel) {
            css_compound_selector_append(comp, sel);
        }
        p++;
    }

    /* 2. Loop subclass selectors */
    while (p < count) {
        /* hash token (type=id) -> SEL_ID */
        if (cv_is(values[p], CSS_TOKEN_HASH)) {
            const char *name = cv_token_value(values[p]);
            if (name) {
                css_simple_selector *sel = css_simple_selector_create(SEL_ID);
                if (sel) {
                    sel->name = strdup(name);
                    css_compound_selector_append(comp, sel);
                }
            }
            p++;
        }
        /* delim('.') + ident -> SEL_CLASS */
        else if (cv_is_delim(values[p], '.') &&
                 p + 1 < count && cv_is(values[p + 1], CSS_TOKEN_IDENT)) {
            const char *name = cv_token_value(values[p + 1]);
            if (name) {
                css_simple_selector *sel = css_simple_selector_create(SEL_CLASS);
                if (sel) {
                    sel->name = strdup(name);
                    css_compound_selector_append(comp, sel);
                }
            }
            p += 2;
        }
        /* simple_block (associated=OPEN_SQUARE) -> attribute selector */
        else if (p < count && values[p]->type == CSS_NODE_SIMPLE_BLOCK &&
                 values[p]->u.block &&
                 values[p]->u.block->associated_token == CSS_TOKEN_OPEN_SQUARE) {
            css_simple_selector *sel = parse_attribute_selector(values[p]->u.block);
            if (sel) {
                css_compound_selector_append(comp, sel);
            }
            p++;
        }
        /* colon + colon + ident -> SEL_PSEUDO_ELEMENT */
        else if (cv_is(values[p], CSS_TOKEN_COLON) &&
                 p + 2 < count &&
                 cv_is(values[p + 1], CSS_TOKEN_COLON) &&
                 cv_is(values[p + 2], CSS_TOKEN_IDENT)) {
            const char *name = cv_token_value(values[p + 2]);
            if (name) {
                css_simple_selector *sel = css_simple_selector_create(SEL_PSEUDO_ELEMENT);
                if (sel) {
                    sel->name = strdup(name);
                    css_compound_selector_append(comp, sel);
                }
            }
            p += 3;
        }
        /* colon + ident -> SEL_PSEUDO_CLASS */
        else if (cv_is(values[p], CSS_TOKEN_COLON) &&
                 p + 1 < count &&
                 cv_is(values[p + 1], CSS_TOKEN_IDENT)) {
            const char *name = cv_token_value(values[p + 1]);
            if (name) {
                css_simple_selector *sel = css_simple_selector_create(SEL_PSEUDO_CLASS);
                if (sel) {
                    sel->name = strdup(name);
                    css_compound_selector_append(comp, sel);
                }
            }
            p += 2;
        }
        else {
            break; /* not a subclass selector */
        }
    }

    /* At least 1 simple selector required */
    if (comp->count == 0) {
        css_compound_selector_free(comp);
        return NULL;
    }

    *pos = p;
    return comp;
}

/* ================================================================
 * Complex selector parsing (Task 5)
 * ================================================================ */

static css_complex_selector *parse_complex_selector(
    css_component_value **values, size_t start, size_t end)
{
    if (!values || start >= end) return NULL;

    css_complex_selector *cx = css_complex_selector_create();
    if (!cx) return NULL;

    size_t pos = start;

    /* Skip leading whitespace */
    while (pos < end && cv_is(values[pos], CSS_TOKEN_WHITESPACE))
        pos++;

    if (pos >= end) {
        css_complex_selector_free(cx);
        return NULL;
    }

    /* Parse first compound selector */
    css_compound_selector *first = parse_compound_selector(values, end, &pos);
    if (!first) {
        css_complex_selector_free(cx);
        return NULL;
    }
    css_complex_selector_append(cx, first, COMB_DESCENDANT); /* comb ignored for first */

    /* Loop: combinator + compound */
    while (pos < end) {
        /* Skip whitespace, record if any */
        bool had_whitespace = false;
        while (pos < end && cv_is(values[pos], CSS_TOKEN_WHITESPACE)) {
            had_whitespace = true;
            pos++;
        }

        if (pos >= end) break;

        /* Check for explicit combinator: >, +, ~ */
        css_combinator comb = COMB_DESCENDANT;
        bool explicit_comb = false;

        if (cv_is_delim(values[pos], '>')) {
            comb = COMB_CHILD;
            explicit_comb = true;
            pos++;
        } else if (cv_is_delim(values[pos], '+')) {
            comb = COMB_NEXT_SIBLING;
            explicit_comb = true;
            pos++;
        } else if (cv_is_delim(values[pos], '~')) {
            comb = COMB_SUBSEQUENT_SIBLING;
            explicit_comb = true;
            pos++;
        } else if (had_whitespace) {
            comb = COMB_DESCENDANT;
        } else {
            /* No whitespace, no combinator — should not happen in valid CSS */
            break;
        }

        /* Skip whitespace after explicit combinator */
        if (explicit_comb) {
            while (pos < end && cv_is(values[pos], CSS_TOKEN_WHITESPACE))
                pos++;
        }

        if (pos >= end) break;

        /* Parse next compound selector */
        css_compound_selector *next = parse_compound_selector(values, end, &pos);
        if (!next) {
            css_complex_selector_free(cx);
            return NULL;
        }
        css_complex_selector_append(cx, next, comb);
    }

    return cx;
}

/* ================================================================
 * Selector list parsing (Task 5)
 * ================================================================ */

css_selector_list *css_parse_selector_list(css_component_value **values,
                                           size_t count)
{
    if (!values || count == 0) return NULL;

    css_selector_list *list = css_selector_list_create();
    if (!list) return NULL;

    /* Split by comma tokens into segments, parse each as complex selector */
    size_t seg_start = 0;

    for (size_t i = 0; i <= count; i++) {
        bool is_comma = (i < count && cv_is(values[i], CSS_TOKEN_COMMA));
        bool is_end = (i == count);

        if (is_comma || is_end) {
            size_t seg_end = i;

            /* Skip empty segments (e.g. leading comma) */
            /* But check if segment has any non-whitespace content */
            bool has_content = false;
            for (size_t j = seg_start; j < seg_end; j++) {
                if (!cv_is(values[j], CSS_TOKEN_WHITESPACE)) {
                    has_content = true;
                    break;
                }
            }

            if (has_content) {
                css_complex_selector *cx = parse_complex_selector(
                    values, seg_start, seg_end);
                if (!cx) {
                    /* Any failure -> entire list is invalid */
                    css_selector_list_free(list);
                    return NULL;
                }
                css_selector_list_append(list, cx);
            }

            seg_start = i + 1;
        }
    }

    /* Empty list -> return NULL */
    if (list->count == 0) {
        css_selector_list_free(list);
        return NULL;
    }

    return list;
}

/* ================================================================
 * Specificity calculation (Task 6)
 * ================================================================ */

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
            case SEL_UNIVERSAL:      break;
            }
        }
    }
    return spec;
}
