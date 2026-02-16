# CSS Parser 知識筆記

## Task 1: 專案骨架與 Makefile

### 背景知識

- **CSS Syntax Module Level 3**: W3C 規範定義了 CSS 的詞法分析（tokenization）和語法分析（parsing）流程
- **兩階段管線**: Tokenizer 將輸入轉為 token 流，Parser 將 token 流組成 AST（抽象語法樹）
- **C11 標準**: 使用 `-std=c11 -Wall -Wextra -pedantic` 嚴格編譯
- **空翻譯單元**: ISO C 不允許空的翻譯單元（translation unit），需要至少一個宣告。使用 placeholder typedef 解決

### 專案結構

```
css_parser/
├── Makefile              # 建構系統
├── include/              # 公開標頭檔
│   ├── css_token.h       # Token 類型定義
│   ├── css_tokenizer.h   # Tokenizer API
│   ├── css_ast.h         # AST 節點定義
│   └── css_parser.h      # Parser API
├── src/                  # 原始碼
│   ├── css_token.c       # Token 生命週期管理
│   ├── css_tokenizer.c   # 核心 tokenizer 狀態機
│   ├── css_ast.c         # AST 節點建立/釋放/傾印
│   ├── css_parser.c      # 核心 parser
│   └── css_parse_demo.c  # CLI 入口點
└── tests/                # 測試檔案
    └── basic.css         # 基本測試用例
```

### Makefile 語法

- `CC ?= cc`: 條件賦值，若未設定則使用 `cc`
- `$@`: 自動變數，代表目標名稱
- `-Iinclude`: 加入標頭檔搜尋路徑
- `-pedantic`: 嚴格遵循 ISO C 標準

### 流程

1. `make css_parse` 編譯所有 .c 檔案為單一執行檔
2. `./css_parse <file>` 讀取 CSS 檔案並（未來）進行解析
3. `make test` 執行基本測試

## Task 2: Token 結構與生命週期

### 背景知識

- **24 種 token 類型**: CSS Syntax Level 3 定義了 ident, function, at-keyword, hash, string, bad-string, url, bad-url, delim, number, percentage, dimension, whitespace, CDO, CDC, colon, semicolon, comma, `[`, `]`, `(`, `)`, `{`, `}`, EOF
- **css_token 結構**: 包含 type、value（字串）、numeric_value、number_type、unit、hash_type、delim_codepoint、line、column
- **css_number_type**: `CSS_NUM_INTEGER` 和 `CSS_NUM_NUMBER` 區分整數與浮點數
- **css_hash_type**: `CSS_HASH_UNRESTRICTED` 和 `CSS_HASH_ID` 區分一般 hash 與 id hash
- **記憶體管理**: `css_token_create()` 使用 `calloc` 分配零初始化，`css_token_free()` 釋放 value 和 unit 字串

### API

- `css_token *css_token_create(css_token_type type)` — 建立新 token
- `void css_token_free(css_token *token)` — 釋放 token 及其字串
- `const char *css_token_type_name(css_token_type type)` — 取得 token 類型名稱

## Task 3: Tokenizer 前處理與 UTF-8 解碼

### 背景知識

- **CSS Syntax §3.3 前處理**: 在 tokenization 之前必須進行輸入前處理
  - CRLF (0x0D 0x0A) → LF (0x0A)：兩個位元組變一個
  - CR (0x0D 單獨出現) → LF (0x0A)
  - FF (0x0C) → LF (0x0A)
  - NULL (0x00) → U+FFFD (0xEF 0xBF 0xBD，UTF-8 三個位元組)
  - 其他字元原樣複製

- **UTF-8 編碼規則**:
  - `0xxxxxxx` → 1 位元組 (ASCII, 0x00–0x7F)
  - `110xxxxx 10xxxxxx` → 2 位元組 (0x80–0x7FF)
  - `1110xxxx 10xxxxxx 10xxxxxx` → 3 位元組 (0x800–0xFFFF)
  - `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` → 4 位元組 (0x10000–0x10FFFF)
  - 無效序列 → U+FFFD，只消耗 1 位元組
  - Overlong 編碼檢查：2 位元組必須 >= 0x80，3 位元組必須 >= 0x800，4 位元組必須 >= 0x10000

- **4 格先行查看管線 (Lookahead Pipeline)**:
  - `current` — 目前消耗的 code point
  - `peek1` — 下一個 code point
  - `peek2` — 下下一個 code point
  - `peek3` — 下下下一個 code point
  - 許多 token 判斷需要向前看 3 個 code point（例如 `starts_number` 需要檢查 3 個字元）

- **CSS_EOF_CODEPOINT**: 定義為 `0xFFFFFFFF`，表示輸入結束

### 語法用法

- `css_tokenizer *css_tokenizer_create(const char *input, size_t length)` — 建立 tokenizer，進行前處理和初始化先行查看
- `css_token *css_tokenizer_next(css_tokenizer *t)` — 取得下一個 token（目前只回傳 EOF）
- `void css_tokenizer_free(css_tokenizer *t)` — 釋放 tokenizer 及其前處理緩衝區

### 內部函式

- `static uint32_t decode_utf8(const char *s, size_t len, size_t *bytes_read)` — 解碼單個 UTF-8 code point
- `static uint32_t peek_at(css_tokenizer *t, size_t byte_pos, size_t *bytes)` — 在指定位元組位置解碼 code point
- `static void fill_lookahead(css_tokenizer *t)` — 初始化時填充 4 格先行查看
- `static void consume_codepoint(css_tokenizer *t)` — 前進一個 code point，更新行/列追蹤
- `static char *preprocess(const char *input, size_t length, size_t *out_len)` — 前處理輸入

### 流程架構

```
原始輸入 → preprocess() 前處理
  ├─ CRLF → LF
  ├─ CR → LF
  ├─ FF → LF
  └─ NULL → U+FFFD (3 bytes)
→ 前處理後的 UTF-8 緩衝區
→ fill_lookahead() 填充 current/peek1/peek2/peek3
→ css_tokenizer_next() 迴圈產生 token（目前只產生 EOF）

consume_codepoint() 推進管線:
  current ← peek1
  peek1   ← peek2
  peek2   ← peek3
  peek3   ← decode_utf8(下一個位置)
  同時更新 line/column（遇到 LF 時 line++, column=1）
```

### CLI --tokens 模式

```bash
./css_parse --tokens tests/basic.css   # 傾印 token 流
./css_parse tests/basic.css            # 預設模式（未來做完整解析）
```

## Task 4: 空白/標點/Comment token

### 背景知識

- **CSS Syntax §4.3.1 Token 分發**: `css_tokenizer_next()` 讀取 `current` code point，根據字元類型分派到不同的 token 產生邏輯
- **CSS Syntax §4.3.2 註解消耗**: 在每次 token 分發前，先檢查是否為 `/*` 開頭的註解，若是則消耗到 `*/` 或 EOF。註解不產生 token（被靜默消耗）
- **未結束的註解**: 若在找到 `*/` 之前遇到 EOF，這是一個解析錯誤（parse error）
- **空白 token**: 連續的空白字元（空格、Tab、換行）合併為單一 `CSS_TOKEN_WHITESPACE`
- **分隔符號 token**: `{`, `}`, `(`, `)`, `[`, `]`, `:`, `;`, `,` 各自對應獨立的 token 類型
- **Delim token**: 目前未處理的字元（字母、數字、`#`、`@` 等）暫時作為 `CSS_TOKEN_DELIM` 輸出，後續 Task 會實作完整的 ident/number/string 等 token

### Code Point 分類輔助函式

```c
is_whitespace(c)    // '\n', '\t', ' '
is_digit(c)         // '0'–'9'
is_hex_digit(c)     // 0–9, A–F, a–f
is_letter(c)        // A–Z, a–z
is_non_ascii(c)     // >= 0x80 且非 EOF
is_ident_start(c)   // 字母、non-ASCII、底線
is_ident_char(c)    // ident_start + 數字 + 連字號
is_non_printable(c) // 0x00–0x08, 0x0B, 0x0E–0x1F, 0x7F
```

### 新增內部函式

- `static void css_parse_error(css_tokenizer *t, const char *msg)` — 若設定 `CSSPARSER_PARSE_ERRORS` 環境變數，印出錯誤訊息到 stderr
- `static void consume_comments(css_tokenizer *t)` — 消耗 `/* ... */` 註解（可連續多個）
- `static css_token *make_token(css_token_type type, size_t line, size_t col)` — 建立帶有行列資訊的 token

### 流程架構

```
css_tokenizer_next():
  1. consume_comments() — 消耗所有連續的註解
  2. 讀取 current code point
  3. 分派:
     - EOF → CSS_TOKEN_EOF
     - 空白 → 消耗連續空白 → CSS_TOKEN_WHITESPACE
     - ( ) [ ] { } : ; , → 對應的標點 token
     - 其他 → CSS_TOKEN_DELIM（暫時）
```

### 測試

```bash
./css_parse --tokens tests/tokens_basic.css   # 測試標點、空白、註解
./css_parse --tokens tests/basic.css          # 測試真實 CSS（字母暫為 delim）
CSSPARSER_PARSE_ERRORS=1 ./css_parse --tokens <未結束註解檔案>  # 測試錯誤報告
```

## Task 5: 數值 token (number/percentage/dimension)

### 背景知識

- **CSS Syntax §4.3.3 consume_numeric_token**: 消耗數字後，根據後續字元判斷產生 number、percentage 或 dimension token
- **CSS Syntax §4.3.7 consume_escaped_codepoint**: 消耗跳脫序列，支援十六進位跳脫（最多 6 位 hex digit）和一般字元跳脫
- **CSS Syntax §4.3.8 valid_escape**: 判斷兩個 code point 是否構成有效跳脫（反斜線後接非換行字元）
- **CSS Syntax §4.3.9 starts_ident_sequence**: 判斷三個 code point 是否能開始一個 ident 序列
- **CSS Syntax §4.3.10 starts_number**: 判斷三個 code point 是否能開始一個數字
- **CSS Syntax §4.3.11 consume_ident_sequence**: 消耗 ident 字元序列，用於讀取 dimension 單位名稱
- **CSS Syntax §4.3.12 consume_number**: 消耗數字，包含可選正負號、整數部分、小數部分、指數部分

### 數字格式

```
[+|-] digits [. digits] [(e|E) [+|-] digits]
```

- 整數: `42`, `-10`, `+5`
- 浮點數: `0.5`, `1.5`, `.5`（前綴零可省略）
- 科學記號: `3e2` = 300, `1.5E-3` = 0.0015

### Token 類型判斷

```
consume_number() → 取得數值
  ├─ 後接 starts_ident_sequence → CSS_TOKEN_DIMENSION（如 100px, 1.5em）
  ├─ 後接 '%' → CSS_TOKEN_PERCENTAGE（如 50%）
  └─ 其他 → CSS_TOKEN_NUMBER（如 42, 0.5）
```

### number_type 區分

- `CSS_NUM_INTEGER`: 沒有小數點且沒有指數部分（如 42, -10）
- `CSS_NUM_NUMBER`: 有小數點或指數部分（如 0.5, 3e2）

### 新增內部函式

- `static bool valid_escape(uint32_t c1, uint32_t c2)` — 判斷有效跳脫
- `static bool starts_number(uint32_t c1, uint32_t c2, uint32_t c3)` — 判斷數字開頭
- `static bool starts_ident_sequence(uint32_t c1, uint32_t c2, uint32_t c3)` — 判斷 ident 序列開頭
- `static size_t encode_utf8(uint32_t cp, char *buf, size_t buf_size)` — 將 code point 編碼為 UTF-8
- `static uint32_t consume_escaped_codepoint(css_tokenizer *t)` — 消耗跳脫 code point
- `static double consume_number(css_tokenizer *t, css_number_type *out_type)` — 消耗數字
- `static char *consume_ident_sequence(css_tokenizer *t)` — 消耗 ident 序列（回傳 strdup 字串）
- `static css_token *consume_numeric_token(css_tokenizer *t)` — 消耗數值 token

### 分發邏輯更新

```
css_tokenizer_next() 分發（在 delim 之前）:
  - 數字 → consume_numeric_token()
  - '+' 且 starts_number → consume_numeric_token()
  - '-' 且 starts_number → consume_numeric_token()
  - '.' 且 starts_number → consume_numeric_token()
```

### `_POSIX_C_SOURCE`

- 為了使用 `strdup()` 函式，需要在 css_tokenizer.c 頂部定義 `#define _POSIX_C_SOURCE 200809L`
- `strdup()` 是 POSIX 函式，不在 C11 標準中，需要此巨集才能在 `-std=c11 -pedantic` 下取得宣告

### 測試

```bash
./css_parse --tokens tests/tokens_numeric.css
# 預期: 100px → <dimension 100 "px">, 50% → <percentage 50>,
#        0.5 → <number 0.5>, 42 → <number 42>, -10px → <dimension -10 "px">,
#        .5 → <number 0.5>, 3e2 → <number 300>
```

### --tokens 增強輸出格式

- `<number 42>` / `<number 3.14>` — 整數顯示為整數，浮點顯示 %g
- `<percentage 50>` — 百分比顯示數值
- `<dimension 10 "px">` — dimension 顯示數值和單位
- `<delim 'c'>` — delim 顯示字元
