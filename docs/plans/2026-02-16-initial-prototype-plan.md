# CSS Parser 初型原型 (P0 + P1) 實作計畫

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 實作完整的 CSS Tokenizer (P0) 和 Parser (P1)，能將任意 CSS 檔案 tokenize 並產出 AST dump。

**Architecture:** 兩階段管道 — Tokenizer 將 CSS 輸入切為 24 種 token 流，Parser 消耗 token 流產出 AST（Stylesheet → Rules → Declarations）。遵循 W3C CSS Syntax Module Level 3 規範。設計風格與 /home/paulboul1013/html_parser/ 一致。

**Tech Stack:** C11, GNU Make, 零外部依賴

---

### Task 1: 建立專案骨架與 Makefile

**Files:**
- Create: `Makefile`
- Create: `include/css_token.h`（空骨架）
- Create: `src/css_parse_demo.c`（最小 main）

**Step 1: 建立目錄結構**

```bash
mkdir -p include src tests
```

**Step 2: 建立 Makefile**

依照 html_parser 的 Makefile 風格建立：

```makefile
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
```

**Step 3: 建立最小 main**

`src/css_parse_demo.c` — 讀取檔案並印出「TODO: parse」：

```c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.css>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    printf("Read %ld bytes from %s\n", len, argv[1]);
    printf("TODO: tokenize and parse\n");

    free(buf);
    return 0;
}
```

**Step 4: 建立空的佔位 source files**

建立 `src/css_token.c`、`src/css_tokenizer.c`、`src/css_ast.c`、`src/css_parser.c` 以及對應 headers 為空骨架（含必要的 include guard），讓 Makefile 能編譯通過。

**Step 5: 建立基本測試檔**

`tests/basic.css`：

```css
body {
    color: red;
    font-size: 16px;
}
```

**Step 6: 驗證編譯與執行**

```bash
make css_parse
./css_parse tests/basic.css
```

預期輸出：`Read XX bytes from tests/basic.css` + `TODO: tokenize and parse`

**Step 7: Commit**

```bash
git add Makefile include/ src/ tests/basic.css
git commit -m "feat: project skeleton with Makefile and CLI entry point"
```

---

### Task 2: Token 結構與生命週期 (css_token.h/c)

**Files:**
- Create: `include/css_token.h`
- Create: `src/css_token.c`

**Step 1: 定義 css_token.h**

依照 CSS_PARSER_PLAN.md §3.1 定義：
- `css_token_type` 枚舉（24 種）
- `css_number_type` 枚舉（INTEGER / NUMBER）
- `css_hash_type` 枚舉（UNRESTRICTED / ID）
- `css_token` 結構體（type, value, numeric_value, number_type, unit, hash_type, delim_codepoint, line, column）

**Step 2: 實作 css_token.c**

- `css_token *css_token_create(css_token_type type)` — calloc + 設 type
- `void css_token_free(css_token *token)` — free value, unit, 然後 free token
- `const char *css_token_type_name(css_token_type type)` — switch/case 回傳字串名稱

**Step 3: 驗證編譯**

```bash
make css_parse
```

預期：編譯成功，無 warning。

**Step 4: Commit**

```bash
git add include/css_token.h src/css_token.c
git commit -m "feat: token structure definition and lifecycle (24 types)"
```

---

### Task 3: Tokenizer 骨架 — 前處理與 UTF-8 解碼 (css_tokenizer.h/c)

**Files:**
- Create: `include/css_tokenizer.h`
- Modify: `src/css_tokenizer.c`

**Step 1: 定義 css_tokenizer.h**

```c
typedef struct {
    char *input;           // 前處理後的副本（owned）
    size_t length;
    size_t pos;            // byte offset

    uint32_t current;      // 當前 code point
    uint32_t peek1;
    uint32_t peek2;
    uint32_t peek3;

    size_t line;
    size_t column;

    bool reconsume;        // reconsume flag
} css_tokenizer;

css_tokenizer *css_tokenizer_create(const char *input, size_t length);
css_token     *css_tokenizer_next(css_tokenizer *t);
void           css_tokenizer_free(css_tokenizer *t);
```

**Step 2: 實作前處理**

在 `css_tokenizer_create()` 中：
1. 配置新 buffer
2. CRLF → LF, CR → LF, FF → LF
3. NULL (U+0000) → U+FFFD（以 UTF-8 3-byte 序列 0xEF 0xBF 0xBD 替換）
4. 初始化 pos=0, line=1, column=1
5. 填入 peek1/peek2/peek3

**Step 3: 實作 UTF-8 解碼**

- `static uint32_t decode_utf8(const char *s, size_t len, size_t *bytes_read)` — 解碼單個 code point
- `static void advance(css_tokenizer *t)` — 推進一個 code point，更新 current/peek/line/column
- `static void fill_lookahead(css_tokenizer *t)` — 填充 peek1/2/3

**Step 4: 實作最小 `css_tokenizer_next()`**

暫時只回傳 EOF token，讓整體可以編譯和呼叫。

**Step 5: 建立 tokenizer 測試用的 token dump 模式**

修改 `css_parse_demo.c`，加入 `--tokens` 模式：讀取檔案 → 建立 tokenizer → 逐一印出 token type + value。

**Step 6: 建立前處理測試檔**

`tests/preprocess.css`：含 CR、CRLF、NULL 字元的 CSS（建立時用程式碼生成）。

**Step 7: 驗證**

```bash
make css_parse
./css_parse --tokens tests/basic.css
```

預期：所有 token 印出 `<EOF>`（因為只實作了 EOF 回傳）。

**Step 8: Commit**

```bash
git add include/css_tokenizer.h src/css_tokenizer.c src/css_parse_demo.c
git commit -m "feat: tokenizer skeleton with preprocessing and UTF-8 decode"
```

---

### Task 4: Tokenizer — 空白、標點、Delim token

**Files:**
- Modify: `src/css_tokenizer.c`

**Step 1: 實作 code point 分類 helper**

```c
static bool is_whitespace(uint32_t c);    // LF, TAB, SPACE
static bool is_digit(uint32_t c);         // 0-9
static bool is_hex_digit(uint32_t c);     // 0-9, A-F, a-f
static bool is_letter(uint32_t c);        // A-Z, a-z
static bool is_non_ascii(uint32_t c);     // >= 0x80
static bool is_ident_start(uint32_t c);   // letter | non-ASCII | _
static bool is_ident(uint32_t c);         // ident-start | digit | -
static bool is_non_printable(uint32_t c); // 0x00-0x08, 0x0B, 0x0E-0x1F, 0x7F
```

**Step 2: 實作簡單 token 消耗**

在 `css_tokenizer_next()` 中加入：
- whitespace → 消耗連續空白 → `<whitespace-token>`
- `(` → `<(-token>`
- `)` → `<)-token>`
- `[` → `<[-token>`
- `]` → `<]-token>`
- `{` → `<{-token>`
- `}` → `<}-token>`
- `:` → `<colon-token>`
- `;` → `<semicolon-token>`
- `,` → `<comma-token>`
- 其他 → `<delim-token>` (value = 該 code point)

**Step 3: 實作 comment 消耗 (§4.3.2)**

`/* ... */` — 未結束時 parse error + 視為結束。

**Step 4: 建立測試檔**

`tests/tokens_basic.css`：

```css
{ } ( ) [ ] : ; ,
/* comment */
```

**Step 5: 驗證**

```bash
make css_parse && ./css_parse --tokens tests/tokens_basic.css
```

預期：正確印出每個 token 的類型。

**Step 6: Commit**

```bash
git add src/css_tokenizer.c tests/tokens_basic.css
git commit -m "feat: tokenizer whitespace, punctuation, delim, and comments"
```

---

### Task 5: Tokenizer — 數值 token (number/percentage/dimension)

**Files:**
- Modify: `src/css_tokenizer.c`

**Step 1: 實作 check helpers**

- `static bool starts_number(uint32_t c1, uint32_t c2, uint32_t c3)` — §4.3.10
- `static bool valid_escape(uint32_t c1, uint32_t c2)` — §4.3.8
- `static bool starts_ident_sequence(uint32_t c1, uint32_t c2, uint32_t c3)` — §4.3.9

**Step 2: 實作 consume_number() (§4.3.12)**

回傳 (value, type) — 支援整數、小數、科學記號。

**Step 3: 實作 consume_numeric_token() (§4.3.3)**

1. 呼叫 consume_number()
2. 若後接 ident-start → consume ident → dimension-token
3. 若後接 `%` → percentage-token
4. 否則 → number-token

**Step 4: 在主分派中加入數值入口**

- digit → reconsume, consume_numeric_token
- `+` → starts_number? consume_numeric_token : delim
- `-` → starts_number? consume_numeric_token : (待後續步驟處理)
- `.` → starts_number? consume_numeric_token : delim

**Step 5: 建立測試檔**

`tests/tokens_numeric.css`：

```css
.test {
    width: 100px;
    height: 50%;
    opacity: 0.5;
    z-index: 42;
    font-size: 1.5em;
    transition: 300ms;
    line-height: 1.2;
    margin: -10px;
    padding: +5px;
    flex: .5;
    scale: 3e2;
}
```

**Step 6: 驗證**

```bash
make css_parse && ./css_parse --tokens tests/tokens_numeric.css
```

預期：正確辨識 number, dimension, percentage token，含正確的 numeric_value 和 unit。

**Step 7: Commit**

```bash
git add src/css_tokenizer.c tests/tokens_numeric.css
git commit -m "feat: tokenizer numeric/percentage/dimension tokens"
```

---

### Task 6: Tokenizer — Ident、Function、At-keyword、Hash token

**Files:**
- Modify: `src/css_tokenizer.c`

**Step 1: 實作 consume_escaped_code_point() (§4.3.7)**

`\` 後接 hex digits → Unicode code point，最多 6 位，後接可選空白。
`\` 後接一般字元 → 該字元本身。

**Step 2: 實作 consume_ident_sequence() (§4.3.11)**

累積 ident chars + escaped code points 為字串。

**Step 3: 實作 consume_ident_like_token() (§4.3.4)**

1. consume ident sequence → name
2. 若 name 為 "url" 且後接 `(` → 消耗 whitespace，判斷引號 vs 非引號 URL
3. 若後接 `(` → function-token
4. 否則 → ident-token

**Step 4: 在主分派中加入入口**

- ident-start char → reconsume, consume_ident_like_token
- `#` → 若後接 ident char 或 valid escape → hash-token（判斷 id/unrestricted type）
- `@` → 若後三字可啟動 ident → consume ident → at-keyword-token
- `\` → valid escape → reconsume, consume_ident_like_token
- `-` → (已有 number 入口) 加上 ident 入口：starts_ident_sequence? consume_ident_like_token

**Step 5: 實作 CDO/CDC**

- `<` → 後三字為 `!--` → CDO token
- `-` → 後二字為 `->` → CDC token

**Step 6: 建立測試檔**

`tests/tokens_ident.css`：

```css
@media screen {
    .container {
        color: red;
        background-color: #fff;
        --custom-var: blue;
    }
    #main {
        display: flex;
    }
}
@import url("style.css");
@charset "UTF-8";
```

**Step 7: 驗證**

```bash
make css_parse && ./css_parse --tokens tests/tokens_ident.css
```

預期：正確辨識 ident, function, at-keyword, hash token。

**Step 8: Commit**

```bash
git add src/css_tokenizer.c tests/tokens_ident.css
git commit -m "feat: tokenizer ident/function/at-keyword/hash/CDO/CDC tokens"
```

---

### Task 7: Tokenizer — String 和 URL token

**Files:**
- Modify: `src/css_tokenizer.c`

**Step 1: 實作 consume_string_token() (§4.3.5)**

- 雙引號或單引號字串
- 支援 escape sequences
- newline → parse error + bad-string-token
- EOF → parse error + 回傳 string-token

**Step 2: 實作 consume_url_token() (§4.3.6)**

- 跳過前導空白
- 消耗到 `)` 或 EOF
- 遇到非法字元（whitespace, `"`, `'`, `(`, non-printable）→ parse error + consume_bad_url_remnants → bad-url-token

**Step 3: 實作 consume_bad_url_remnants() (§4.3.14)**

跳到 `)` 或 EOF。

**Step 4: 在主分派加入入口**

- `"` → consume_string_token (ending = `"`)
- `'` → consume_string_token (ending = `'`)
- consume_ident_like_token 中 url( 分支 → consume_url_token

**Step 5: 建立測試檔**

`tests/tokens_string.css`：

```css
.test {
    content: "hello world";
    content: 'single quotes';
    background: url(image.png);
    background: url("quoted.png");
    font-family: "Helvetica Neue", Arial;
    content: "escaped \"quote\"";
    content: "line\Acontinued";
}
```

**Step 6: 驗證**

```bash
make css_parse && ./css_parse --tokens tests/tokens_string.css
```

預期：正確辨識 string-token、url-token。

**Step 7: Commit**

```bash
git add src/css_tokenizer.c tests/tokens_string.css
git commit -m "feat: tokenizer string and URL tokens"
```

---

### Task 8: Tokenizer 完整性驗證

**Files:**
- Modify: `src/css_parse_demo.c`（改進 token dump 格式）
- Create: `tests/tokens.css`（綜合測試）
- Create: `tests/errors.css`（錯誤回復測試）

**Step 1: 改進 token dump 輸出格式**

讓 `--tokens` 模式印出更詳細的資訊：
```
[1:1] <whitespace>
[1:1] <ident "body">
[1:5] <{>
[2:5] <ident "color">
[2:10] <colon>
[2:12] <ident "red">
[2:15] <semicolon>
```

**Step 2: 建立綜合測試檔**

`tests/tokens.css` — 涵蓋所有 24 種 token 類型。

**Step 3: 建立錯誤測試檔**

`tests/errors.css` — 未結束字串、未結束 comment、未結束 URL、bad URL、lone `\`。

**Step 4: 驗證全部 token 類型正確**

```bash
make css_parse
./css_parse --tokens tests/tokens.css
CSSPARSER_PARSE_ERRORS=1 ./css_parse --tokens tests/errors.css
```

**Step 5: Commit**

```bash
git add src/css_parse_demo.c tests/tokens.css tests/errors.css
git commit -m "feat: tokenizer comprehensive verification and error recovery"
```

---

### Task 9: AST 結構定義 (css_ast.h/c)

**Files:**
- Create: `include/css_ast.h`
- Modify: `src/css_ast.c`

**Step 1: 定義 AST 節點結構**

依照 CSS_PARSER_PLAN.md §3.2，在 `css_ast.h` 中定義：
- `css_node_type` 枚舉
- Forward declarations（解決循環依賴）
- `css_component_value`, `css_simple_block`, `css_function` 結構
- `css_declaration` 結構（name, value[], important）
- `css_at_rule` 結構（name, prelude[], block）
- `css_qualified_rule` 結構（prelude[], block）
- `css_rule` union wrapper
- `css_stylesheet` 結構（rules[]）

**Step 2: 實作 create 函式**

每個節點類型一個 `css_*_create()` 函式。
動態陣列使用 realloc 增長（每次 *2）。

**Step 3: 實作 free 函式**

遞迴釋放：
- `css_stylesheet_free()` → 遞迴 free 所有 rules
- `css_rule_free()` → free at_rule 或 qualified_rule
- `css_component_value_free()` → free token / block / function
- 每個函式處理 NULL 安全

**Step 4: 實作 css_ast_dump()**

樹狀格式印出，使用 `├──` `└──` 縮排。參考 CSS_PARSER_PLAN.md §11.3 的格式。

**Step 5: 驗證編譯**

```bash
make css_parse
```

**Step 6: Commit**

```bash
git add include/css_ast.h src/css_ast.c
git commit -m "feat: AST node structures with create/free/dump"
```

---

### Task 10: Parser 骨架 (css_parser.h/c)

**Files:**
- Create: `include/css_parser.h`
- Modify: `src/css_parser.c`

**Step 1: 定義 css_parser.h**

```c
css_stylesheet *css_parse_stylesheet(const char *input, size_t length);
void            css_stylesheet_free(css_stylesheet *sheet);
```

**Step 2: 實作 parser 內部結構**

```c
typedef struct {
    css_tokenizer *tokenizer;
    css_token *current_token;
    bool reconsume;
} css_parser;
```

- `static css_token *next_token(css_parser *p)` — 處理 reconsume flag
- `static void reconsume_token(css_parser *p)` — 設 reconsume = true

**Step 3: 實作 css_parse_stylesheet()**

1. 建立 tokenizer
2. 呼叫 consume_list_of_rules（top_level = true）
3. 包裝為 css_stylesheet
4. 清理 tokenizer

**Step 4: 實作 consume_list_of_rules() (§5.4.1)**

```
重複：
  whitespace → 忽略
  EOF → 回傳
  CDO/CDC → top-level? 忽略 : reconsume → consume_qualified_rule
  at-keyword → reconsume → consume_at_rule
  其他 → reconsume → consume_qualified_rule
```

**Step 5: 實作 consume_component_value() (§5.4.7)**

```
{/[/( → consume_simple_block
function → consume_function
其他 → 包裝為 component_value (token)
```

**Step 6: 實作 consume_simple_block() (§5.4.8)**

記錄 mirror token，消耗內容直到匹配的 close 或 EOF。

**Step 7: 實作 consume_function() (§5.4.9)**

建立 function node，消耗到 `)` 或 EOF。

**Step 8: 驗證編譯**

```bash
make css_parse
```

**Step 9: Commit**

```bash
git add include/css_parser.h src/css_parser.c
git commit -m "feat: parser skeleton with consume rules/component/block/function"
```

---

### Task 11: Parser — at-rule 和 qualified rule 消耗

**Files:**
- Modify: `src/css_parser.c`

**Step 1: 實作 consume_at_rule() (§5.4.2)**

```
name = at-keyword 值
重複：
  ; → 回傳
  EOF → parse error, 回傳
  { → consume_simple_block → block → 回傳
  其他 → reconsume → consume_component_value → 加入 prelude
```

**Step 2: 實作 consume_qualified_rule() (§5.4.3)**

```
重複：
  EOF → parse error, 回傳 NULL
  { → consume_simple_block → block → 回傳
  其他 → reconsume → consume_component_value → 加入 prelude
```

**Step 3: 連接 CLI**

修改 `css_parse_demo.c`：預設模式呼叫 `css_parse_stylesheet()` + `css_ast_dump()`。

**Step 4: 驗證**

```bash
make css_parse && ./css_parse tests/basic.css
```

預期：看到 AST dump 輸出（STYLESHEET → QUALIFIED_RULE → BLOCK）。

**Step 5: Commit**

```bash
git add src/css_parser.c src/css_parse_demo.c
git commit -m "feat: parser at-rule and qualified rule consumption"
```

---

### Task 12: Parser — Declaration 消耗與 !important

**Files:**
- Modify: `src/css_parser.c`

**Step 1: 實作 consume_list_of_declarations() (§5.4.5)**

```
重複：
  whitespace / ; → 忽略
  EOF → 回傳
  at-keyword → reconsume → consume_at_rule
  ident → reconsume → consume_declaration
  其他 → parse error, 跳到 ; 或 EOF
```

**Step 2: 實作 consume_declaration() (§5.4.6)**

```
1. 消耗 ident → name
2. 跳過 whitespace
3. 非 colon → parse error, 回傳 NULL
4. 消耗 colon, 跳過 whitespace
5. 消耗 component values 到 EOF → value
6. 檢查末尾 ! + important → 設 flag
7. 移除尾部 whitespace
8. 回傳
```

**Step 3: 連接 qualified rule block 解析**

在 `css_parse_stylesheet()` 後處理：對每個 qualified_rule 的 block，將其 values 重新解析為 declaration list。

**Step 4: 建立測試檔**

`tests/declarations.css`：

```css
body {
    color: red;
    font-size: 16px;
    margin: 0 auto;
    background: #fff !important;
}

@media screen {
    .container {
        max-width: 1200px;
    }
}
```

**Step 5: 驗證**

```bash
make css_parse && ./css_parse tests/declarations.css
```

預期 AST dump：

```
STYLESHEET
├── QUALIFIED_RULE
│   ├── prelude: <ident "body">
│   └── BLOCK {}
│       ├── DECLARATION "color"
│       │   └── value: <ident "red">
│       ├── DECLARATION "font-size"
│       │   └── value: <dimension 16 "px">
│       ├── DECLARATION "margin"
│       │   └── value: <number 0> <ident "auto">
│       └── DECLARATION "background" [!important]
│           └── value: <hash "fff">
├── AT_RULE "@media"
│   ├── prelude: <ident "screen">
│   └── BLOCK {}
│       └── QUALIFIED_RULE
│           ...
```

**Step 6: Commit**

```bash
git add src/css_parser.c tests/declarations.css
git commit -m "feat: parser declaration consumption with !important detection"
```

---

### Task 13: 端到端整合與錯誤回復測試

**Files:**
- Modify: `src/css_parse_demo.c`
- Create: `tests/at_rules.css`
- Modify: `tests/errors.css`

**Step 1: 完善 CLI**

```
./css_parse <file>              # Parse + AST dump（預設）
./css_parse --tokens <file>     # Token dump
CSSPARSER_PARSE_ERRORS=1 ./css_parse <file>  # 含 parse error 報告
```

**Step 2: 建立 at-rule 測試檔**

`tests/at_rules.css`：

```css
@charset "UTF-8";
@import url("reset.css");

@media (max-width: 768px) {
    body { font-size: 14px; }
}

@font-face {
    font-family: "MyFont";
    src: url("font.woff2");
}

@keyframes fadeIn {
    from { opacity: 0; }
    to { opacity: 1; }
}
```

**Step 3: 擴充 errors.css 測試 parser 錯誤回復**

```css
/* 缺少 { 的 rule */
body color: red;

/* 缺少 : 的 declaration */
.test { color red; }

/* 未結束的 block */
div { margin: 10px;

/* 缺少 ; 的 at-rule */
@import url("test.css")

/* 正常的 rule（確認回復後能繼續解析） */
p { color: blue; }
```

**Step 4: 驗證**

```bash
make css_parse
./css_parse tests/at_rules.css
./css_parse tests/declarations.css
CSSPARSER_PARSE_ERRORS=1 ./css_parse tests/errors.css
```

**Step 5: 更新 Makefile test targets**

```makefile
test: css_parse
	./css_parse tests/basic.css
	./css_parse tests/declarations.css
	./css_parse tests/at_rules.css

test-tokens: css_parse
	./css_parse --tokens tests/tokens.css

test-errors: css_parse
	CSSPARSER_PARSE_ERRORS=1 ./css_parse tests/errors.css
```

**Step 6: Commit**

```bash
git add src/css_parse_demo.c tests/ Makefile
git commit -m "feat: end-to-end integration with error recovery tests"
```

---

### Task 14: 記憶體驗證與文件更新

**Files:**
- Modify: `CLAUDE.md`（更新為實際狀態）
- Create: `list.md`（追蹤完成狀態）
- Create: `know.md`（背景知識記錄）

**Step 1: 記憶體洩漏檢查**

```bash
valgrind --leak-check=full ./css_parse tests/basic.css
valgrind --leak-check=full ./css_parse tests/at_rules.css
```

修復任何洩漏。

**Step 2: 更新 CLAUDE.md**

反映實際的建置指令和已完成功能。

**Step 3: 建立 list.md**

追蹤 P0/P1/P2/P3 各功能的完成狀態。

**Step 4: 建立 know.md**

記錄實作過程中累積的背景知識（CSS tokenizer 狀態機、UTF-8 處理、AST 結構等）。

**Step 5: 最終驗證**

```bash
make clean && make css_parse
./css_parse tests/basic.css
./css_parse tests/declarations.css
./css_parse tests/at_rules.css
./css_parse --tokens tests/tokens.css
CSSPARSER_PARSE_ERRORS=1 ./css_parse tests/errors.css
```

**Step 6: Commit**

```bash
git add CLAUDE.md list.md know.md
git commit -m "docs: update project documentation and tracking files"
```

---

## 任務依賴關係

```
Task 1 (骨架)
  → Task 2 (Token 結構)
    → Task 3 (Tokenizer 前處理/UTF-8)
      → Task 4 (空白/標點)
        → Task 5 (數值)
          → Task 6 (Ident/Hash/At)
            → Task 7 (String/URL)
              → Task 8 (Tokenizer 驗證)
                → Task 9 (AST 結構)
                  → Task 10 (Parser 骨架)
                    → Task 11 (at/qualified rule)
                      → Task 12 (Declaration)
                        → Task 13 (整合測試)
                          → Task 14 (驗證/文件)
```

完全線性依賴：每個 Task 依賴前一個。
