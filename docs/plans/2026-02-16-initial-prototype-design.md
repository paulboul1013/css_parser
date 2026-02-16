# CSS Parser 初型原型設計 (P0 + P1)

日期：2026-02-16

## 範圍

實作 P0 (Tokenizer) + P1 (Parser)，能將 CSS 檔案 tokenize 並產生 AST dump 輸出。

## 實作策略

方案 A：嚴格按計畫順序 — 完整 Tokenizer → 完整 Parser，每階段獨立驗證。

## 檔案結構

```
css_parser/
├── include/
│   ├── css_token.h        # Token 類型枚舉 + css_token 結構
│   ├── css_tokenizer.h    # css_tokenizer 結構 + API
│   ├── css_ast.h          # AST 節點結構
│   └── css_parser.h       # Parser API
├── src/
│   ├── css_token.c        # token_init / token_free / token_type_name
│   ├── css_tokenizer.c    # 完整 tokenizer 狀態機
│   ├── css_ast.c          # AST 節點 create/free/dump
│   ├── css_parser.c       # 核心 parser
│   └── css_parse_demo.c   # CLI 入口
├── tests/
│   ├── basic.css
│   ├── tokens.css
│   └── errors.css
└── Makefile
```

## Makefile

- `make css_parse` — 編譯所有 src/*.c 為 `./css_parse`
- `make clean` — 清除產物
- 旗標：`-std=c11 -Wall -Wextra -pedantic -g -Iinclude`

## P0：Tokenizer 設計

### API

```c
css_tokenizer *css_tokenizer_create(const char *input, size_t length);
css_token     *css_tokenizer_next(css_tokenizer *t);
void           css_tokenizer_free(css_tokenizer *t);
void           css_token_free(css_token *token);
const char    *css_token_type_name(css_token_type type);
```

### 前處理

在 `css_tokenizer_create()` 中完成 CR/CRLF→LF、NULL→U+FFFD 替換。

### UTF-8

逐 byte 解碼為 code point，維護 current + peek1/peek2/peek3。

### Token 消耗

完整實作 §4.3.1 分派邏輯，24 種 token。包含所有子演算法：
- Comment (§4.3.2)
- Numeric token (§4.3.3)
- Ident-like token (§4.3.4)
- String token (§4.3.5)
- URL token (§4.3.6)
- Escaped code point (§4.3.7)
- Helpers: valid escape, start ident, start number, consume ident, consume number (§4.3.8–§4.3.14)

### 錯誤報告

`CSSPARSER_PARSE_ERRORS=1` 環境變數控制 stderr 輸出。

## P1：Parser 設計

### API

```c
css_stylesheet *css_parse_stylesheet(const char *input, size_t length);
void            css_stylesheet_free(css_stylesheet *sheet);
void            css_ast_dump(css_stylesheet *sheet, FILE *out);
```

### AST 節點

依照計畫 §5：stylesheet, rule (union: at_rule | qualified_rule), at_rule, qualified_rule, declaration, component_value (union: token | simple_block | function), simple_block, function。

### Consume 演算法 (§5.4)

1. consume_list_of_rules — top-level flag
2. consume_at_rule
3. consume_qualified_rule
4. consume_list_of_declarations
5. consume_declaration — 含 !important 偵測
6. consume_component_value
7. consume_simple_block
8. consume_function

### 記憶體管理

每個 create 有對應 free，AST 銷毀時遞迴釋放所有子節點。

### AST Dump

樹狀格式輸出（├── └──），可到 stdout 或 FILE*。
