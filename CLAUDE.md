# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A spec-compliant CSS parser implemented in pure C11, following [W3C CSS Syntax Module Level 3](https://www.w3.org/TR/css-syntax-3/). Zero external dependencies — standard library only. Designed as a companion to an existing HTML parser project with matching design patterns.

## Build & Run Commands

```sh
make css_parse                                          # Build the parser
./css_parse tests/basic.css                             # Parse a CSS file and dump AST
./css_parse --tokens tests/basic.css                    # Dump token stream only
CSSPARSER_PARSE_ERRORS=1 ./css_parse tests/errors.css   # Parse with error reporting enabled
make test                                               # Run basic parse tests
make test-tokens                                        # Run token dump tests
make test-errors                                        # Run error recovery tests
make test-selectors                                     # Run selector parsing tests
make test-all                                           # Run all tests
```

## Architecture

Two-stage pipeline: **Tokenizer → Parser**, with additional Selector and Value parsing layers.

```
CSS Input → Preprocessing (CR/FF/CRLF→LF, NULL→U+FFFD)
         → Tokenizer (24 token types, state machine in css_tokenizer.c)
         → Parser (produces AST: Stylesheet→Rules→Declarations, in css_parser.c)
         → Selector Parser (CSS Selectors Level 4, in css_selector.c)
         → Value Parser (CSS Values and Units Level 4, in css_value.c)
```

**Implemented source files (P0 + P1 + P2a):**
- `include/css_token.h` — Token types (24 types) + css_token struct
- `include/css_tokenizer.h` — Tokenizer API + css_tokenizer struct
- `include/css_ast.h` — AST node types (7 types) + all struct definitions
- `include/css_parser.h` — Parser API (css_parse_stylesheet)
- `include/css_selector.h` — Selector types + parsing API
- `src/css_token.c` — Token lifecycle (create/free/type_name)
- `src/css_tokenizer.c` — Complete tokenizer state machine (~660 lines)
- `src/css_ast.c` — AST node create/free/dump (~460 lines)
- `src/css_parser.c` — Core parser with consume-based algorithms (~740 lines)
- `src/css_selector.c` — Selector parsing + specificity calculation (~500 lines)
- `src/css_parse_demo.c` — CLI entry point (--tokens mode + default parse mode)

**Planned (not yet implemented):**
- `src/css_value.c` — Value parsing (P3)

## Design Principles

- **Forgiving parsing**: CSS spec requires error-tolerant parsing — skip invalid content, never abort
- **Memory safety**: All strings use `strdup()`, every allocation has a matching `free` path
- **Incremental implementation**: P0 (Tokenizer) → P1 (Parser) → P2 (Selectors) → P3 (Advanced features like nesting, media queries, calc())
- **Language**: C11 standard, no external dependencies

## Current Implementation Status

- **P0 (Tokenizer)**: Complete — all 24 token types, preprocessing, UTF-8, escape sequences, error recovery
- **P1 (Parser)**: Complete — stylesheet/rules/declarations/blocks/functions, !important detection, AST dump
- **P2a (Selectors Core)**: Complete — type/universal/class/id/attribute(7)/pseudo-class/pseudo-element, 4 combinators, specificity, integrated into parser
- **P3 (Advanced)**: Not started

## Specifications Implemented

Primary: CSS Syntax Module Level 3 (§3-§5 complete). Planned: CSS Selectors Level 4, CSS Values and Units Level 4, CSS Cascade Level 5, CSS Conditional Rules Level 3, Media Queries Level 5, CSS Nesting, CSS Color Level 4.

## Reference

See `CSS_PARSER_PLAN.md` for the full implementation plan including detailed data structures, algorithms, and phased implementation schedule.

## 注意
用繁體中文思考和回答
每進行一步寫代碼操作都要進行背景知識的補充，並且都要更新知識，語法用法，流程架構，到know.md
每做一步都要進行測資測試，並且測資都要更新測資到tests資料夾
列出現在未更新和已更新的內容到list.md，並且隨時更新是否完成以及增添新內容
請隨時注意html_parser/的內容，因為你們兩個專案是未來將會一起變成render tree輸出

