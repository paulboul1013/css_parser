#ifndef CSS_PARSER_H
#define CSS_PARSER_H

#include "css_ast.h"

/* Parse a CSS stylesheet from input string */
css_stylesheet *css_parse_stylesheet(const char *input, size_t length);

#endif /* CSS_PARSER_H */
