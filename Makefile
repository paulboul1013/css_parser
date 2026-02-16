CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -O2 -g

SRC = src/css_token.c src/css_tokenizer.c src/css_ast.c src/css_parser.c

all: css_parse

css_parse: $(SRC) src/css_parse_demo.c
	$(CC) $(CFLAGS) -Iinclude $(SRC) src/css_parse_demo.c -o $@

clean:
	rm -f css_parse

test: css_parse
	./css_parse tests/basic.css
