CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -O2 -g

SRC = src/css_token.c src/css_tokenizer.c src/css_ast.c src/css_parser.c src/css_selector.c

all: css_parse

css_parse: $(SRC) src/css_parse_demo.c
	$(CC) $(CFLAGS) -Iinclude $(SRC) src/css_parse_demo.c -o $@

clean:
	rm -f css_parse

test: css_parse
	./css_parse tests/basic.css
	./css_parse tests/declarations.css
	./css_parse tests/at_rules.css

test-tokens: css_parse
	./css_parse --tokens tests/tokens.css

test-errors: css_parse
	CSSPARSER_PARSE_ERRORS=1 ./css_parse tests/errors.css

test-selectors: css_parse
	./css_parse tests/selectors.css

test-all: test test-tokens test-errors test-selectors
