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

## Task 6: Ident/Function/Hash/At token

### 背景知識

- **CSS Syntax §4.3.4 consume_ident_like_token**: 消耗 ident 序列後，判斷是否為函式呼叫（後接 `(`）或一般 ident token
- **url() 特殊處理**: 若 ident 為 `url`（不分大小寫）且後接 `(`，需進一步判斷：
  - 引號開頭 → function token（url 作為函式名稱，字串作為參數）
  - 非引號 → url-token（Task 7 實作）
- **Hash token (#)**: `#` 後接 ident 字元或有效跳脫 → hash token，若 starts_ident_sequence 則為 id 類型
- **At-keyword (@)**: `@` 後接 starts_ident_sequence → at-keyword token
- **CDC (--)>**: `-->` 三個字元 → CDC token
- **CDO (<!--)**: `<!--` 四個字元 → CDO token
- **反斜線跳脫**: `\` 後接有效跳脫 → 開始 ident-like token

### 新增內部函式

- `static css_token *consume_ident_like_token(css_tokenizer *t)` — 消耗 ident-like token（ident、function、url）

### 分發邏輯更新

```
css_tokenizer_next() 分發:
  - '#' 後接 ident 字元/跳脫 → hash token
  - '+' 且 starts_number → numeric token
  - '-' → number / CDC / ident / delim
  - '.' 且 starts_number → numeric token
  - '<' 且 !-- → CDO token
  - '@' 後接 starts_ident_sequence → at-keyword token
  - '\' 後接有效跳脫 → ident-like token
  - ident_start → ident-like token
```

## Task 7: String 和 URL token

### 背景知識

- **CSS Syntax §4.3.5 consume_string_token**: 消耗字串 token（雙引號或單引號包圍的內容）
  - 遇到配對的結束引號 → 回傳 string-token
  - 遇到 EOF → parse error，回傳已收集的 string-token
  - 遇到未跳脫的換行 → parse error，回傳 bad-string-token（不消耗換行）
  - `\\` + EOF → 消耗反斜線，繼續
  - `\\` + `\n` → 跳脫換行（行接續），消耗兩者但不加入字串值
  - `\\` + 其他 → 有效跳脫，使用 consume_escaped_codepoint 解碼

- **CSS Syntax §4.3.6 consume_url_token**: 消耗不帶引號的 URL token
  - 由 consume_ident_like_token 呼叫，當 `url(` 後接非引號字元時
  - `url(` 後的空白已被跳過
  - `)` → 結束 URL token
  - 空白 → 跳過空白後，只能接 `)` 或 EOF，否則為 bad-url
  - `"`, `'`, `(`, non-printable 字元 → bad-url
  - `\` + 有效跳脫 → 消耗跳脫字元
  - `\` + 無效跳脫 → bad-url
  - EOF → parse error，回傳已收集的 url-token

- **CSS Syntax §4.3.14 consume_bad_url_remnants**: 消耗 bad URL 的剩餘部分
  - 消耗直到遇到 `)` 或 EOF
  - 若遇到有效跳脫則消耗跳脫序列

- **字串分發**: `"` 和 `'` 在主分發迴圈中觸發 consume_string_token，傳入結束引號字元

### 新增內部函式

- `static void consume_bad_url_remnants(css_tokenizer *t)` — 消耗 bad URL 剩餘字元
- `static css_token *consume_string_token(css_tokenizer *t, uint32_t ending)` — 消耗字串 token
- `static css_token *consume_url_token(css_tokenizer *t, size_t tok_line, size_t tok_col)` — 消耗不帶引號的 URL token

### Token 類型

- `CSS_TOKEN_STRING` — 有效的字串（value 為字串內容，不含引號）
- `CSS_TOKEN_BAD_STRING` — 字串中遇到未跳脫換行
- `CSS_TOKEN_URL` — 不帶引號的 URL（value 為 URL 內容）
- `CSS_TOKEN_BAD_URL` — URL 中遇到無效字元

### URL 處理流程

```
consume_ident_like_token():
  名稱為 "url" 且後接 '(' →
    跳過空白 →
      ├─ 引號 ('"' 或 '\'') → function token (url)，之後 parser 會讀取 string token
      └─ 非引號 → consume_url_token() → url-token 或 bad-url-token
```

### --tokens 增強輸出格式

- `<string "hello world">` — 字串 token（顯示 value）
- `<bad-string>` — 壞字串 token
- `<url "image.png">` — URL token（顯示 value）
- `<bad-url>` — 壞 URL token

### 測試

```bash
./css_parse --tokens tests/tokens_string.css
# 預期:
#   "hello world" → <string "hello world">
#   'single quotes' → <string "single quotes">
#   url(image.png) → <url "image.png">
#   url("quoted.png") → <function "url"> + <string "quoted.png"> + <)>
#   "Helvetica Neue" → <string "Helvetica Neue">
#   "escaped \"quote\"" → <string "escaped "quote"">
```

## Task 9: AST 結構定義

### 背景知識

- **CSS Syntax §5**: 定義了 CSS 解析產生的抽象語法樹（AST）結構
- **AST 節點類型**: 7 種節點類型
  - `CSS_NODE_STYLESHEET` — 頂層節點，包含規則列表
  - `CSS_NODE_AT_RULE` — @規則（如 @media, @import）
  - `CSS_NODE_QUALIFIED_RULE` — 合格規則（如選擇器 + 宣告區塊）
  - `CSS_NODE_DECLARATION` — 宣告（如 color: red）
  - `CSS_NODE_COMPONENT_VALUE` — 元件值（保留的 token）
  - `CSS_NODE_SIMPLE_BLOCK` — 簡單區塊（{}, [], ()）
  - `CSS_NODE_FUNCTION` — 函式（如 rgb(), calc()）

- **CSS Syntax §5.3 Component Value**: 是 token / simple block / function 三者的聯合體
  - 使用 `css_node_type` 判斷實際類型
  - union 包含 token 指標、block 指標、function 指標

- **CSS Syntax §5.4.8 Simple Block**: 由配對的括號包圍，包含子元件值列表
  - `associated_token` 記錄開啟括號類型（{, [, (）
  - 值列表使用動態陣列

- **CSS Syntax §5.4.9 Function**: 名稱 + 參數值列表

- **CSS Syntax §5.4.6 Declaration**: 屬性名 + 值列表 + !important 旗標

- **CSS Syntax §5.4.2 At-rule**: 名稱 + prelude（前序元件值列表）+ 可選的 block
  - 語句式 at-rule（如 @import）沒有 block，以分號結束
  - 區塊式 at-rule（如 @media）有 block

- **CSS Syntax §5.4.3 Qualified Rule**: prelude（選擇器）+ block（宣告區塊）

- **Rule 包裝器**: css_rule 使用 union 包裝 at-rule 或 qualified rule

### API 概覽

#### 建立函式

```c
css_stylesheet *css_stylesheet_create(void);
css_rule *css_rule_create_at(css_at_rule *ar);
css_rule *css_rule_create_qualified(css_qualified_rule *qr);
css_at_rule *css_at_rule_create(const char *name);
css_qualified_rule *css_qualified_rule_create(void);
css_declaration *css_declaration_create(const char *name);
css_simple_block *css_simple_block_create(css_token_type associated);
css_function *css_function_create(const char *name);
css_component_value *css_component_value_create_token(css_token *token);
css_component_value *css_component_value_create_block(css_simple_block *block);
css_component_value *css_component_value_create_function(css_function *func);
```

- 所有建立函式使用 `calloc` 零初始化
- 字串欄位使用 `strdup()` 取得擁有權

#### 釋放函式

```c
void css_stylesheet_free(css_stylesheet *sheet);
void css_rule_free(css_rule *rule);
void css_at_rule_free(css_at_rule *ar);
void css_qualified_rule_free(css_qualified_rule *qr);
void css_declaration_free(css_declaration *decl);
void css_simple_block_free(css_simple_block *block);
void css_function_free(css_function *func);
void css_component_value_free(css_component_value *cv);
```

- 全部 NULL 安全
- 遞迴釋放子節點
- `css_component_value_free` 根據 type 分派到正確的釋放函式

#### 附加輔助函式（動態陣列）

```c
void css_stylesheet_append_rule(css_stylesheet *sheet, css_rule *rule);
void css_at_rule_append_prelude(css_at_rule *ar, css_component_value *cv);
void css_qualified_rule_append_prelude(css_qualified_rule *qr, css_component_value *cv);
void css_simple_block_append_value(css_simple_block *block, css_component_value *cv);
void css_function_append_value(css_function *func, css_component_value *cv);
void css_declaration_append_value(css_declaration *decl, css_component_value *cv);
```

- 使用 realloc 擴容模式：`cap = cap ? cap * 2 : 4`
- 全部 NULL 安全（傳入 NULL 不會 crash）

#### 傾印函式

```c
void css_ast_dump(css_stylesheet *sheet, FILE *out);
```

- 以縮排形式輸出 AST 結構
- 每層深度增加兩個空格

### 記憶體管理模式

```
calloc(1, sizeof(type))     — 零初始化，cap/count 自動為 0
strdup(name)                — 字串擁有權轉移
realloc(ptr, new_cap * sz)  — 動態陣列擴容
free(ptr->name)             — 釋放 strdup 的字串
free(ptr->values)           — 釋放動態陣列
free(ptr)                   — 釋放節點本身
```

### 傾印輸出格式

```
STYLESHEET
  QUALIFIED_RULE
    prelude:
      <ident "body">
    BLOCK {}
      <ident "color">
      <ident "red">
  AT_RULE "media"
    prelude:
      <ident "screen">
    BLOCK {}
```

### 測試

```bash
cc -std=c11 -Wall -Wextra -pedantic -O2 -g -Iinclude src/css_token.c src/css_ast.c tests/test_ast.c -o tests/test_ast
./tests/test_ast
# 測試: create/free/append/dump/NULL 安全性
```

## Task 8: Tokenizer 完整性驗證

### 背景知識

- **Token 傾印增強**: 加上 `[line:col]` 前綴，方便除錯定位
- **完整測試檔案**: `tests/tokens.css` 包含所有 24 種 token 類型的測試用例
- **錯誤測試**: `tests/errors.css` 測試錯誤恢復場景（未結束的註解、bad-string、bad-url）

### 完整的 --tokens 輸出格式

```
[1:1] <ident "body">
[1:5] <whitespace>
[1:6] <{>
[2:5] <ident "color">
[2:10] <:>
[2:12] <ident "red">
[2:15] <;>
[3:1] <}>
[3:2] <EOF>
```

### 24 種 token 的輸出格式對照

| Token 類型 | 輸出格式 |
|-----------|---------|
| ident | `<ident "name">` |
| function | `<function "name">` |
| at-keyword | `<at-keyword "name">` |
| hash (id) | `<hash id "name">` |
| hash (unrestricted) | `<hash "name">` |
| string | `<string "value">` |
| bad-string | `<bad-string>` |
| url | `<url "value">` |
| bad-url | `<bad-url>` |
| number (integer) | `<number 42>` |
| number (float) | `<number 3.14>` |
| percentage | `<percentage 50>` |
| dimension | `<dimension 10 "px">` |
| whitespace | `<whitespace>` |
| CDO | `<CDO>` |
| CDC | `<CDC>` |
| colon | `<:>` |
| semicolon | `<;>` |
| comma | `<,>` |
| delim | `<delim 'c'>` |
| 6 種括號 | `<(>`, `<)>`, `<[>`, `<]>`, `<{>`, `<}>` |
| EOF | `<EOF>` |

## Task 10-12: CSS Parser (P1) — consume-based 演算法

### 背景知識

- **CSS Syntax Module Level 3 §5**: 定義了 CSS 解析的核心演算法
- **consume-based 演算法**: Parser 從 tokenizer 逐一消耗 token，根據 token 類型分派到不同的消耗函式
- **兩階段解析**: 先建立原始 AST（規則 + 區塊 + 元件值），再從區塊內容解析宣告

### 內部結構

```c
typedef struct {
    css_tokenizer *tokenizer;
    css_token *current_token;  /* 目前消耗的 token（擁有權） */
    bool reconsume;            /* 重新消耗旗標 */
} css_parser_ctx;
```

### Token 消耗機制

- `next_token(p)`: 取得下一個 token。若 reconsume 旗標為 true，回傳目前 token
- `reconsume(p)`: 設定 reconsume 旗標，下次 next_token 不消耗新 token
- `clone_token(src)`: 深拷貝 token（strdup value/unit）

### 核心消耗函式 (CSS Syntax §5.4)

1. **consume_list_of_rules (§5.4.1)**: 頂層迴圈
   - 跳過空白
   - EOF → 結束
   - CDO/CDC → top_level 時跳過，否則當作 qualified rule
   - at-keyword → consume_at_rule
   - 其他 → consume_qualified_rule

2. **consume_at_rule (§5.4.2)**: @規則
   - 名稱來自 at-keyword token
   - 消耗 prelude 直到 `;`、`{`、或 EOF
   - 遇到 `{` → consume_simple_block 作為 block

3. **consume_qualified_rule (§5.4.3)**: 合格規則
   - 消耗 prelude 直到 `{` 或 EOF
   - 遇到 `{` → consume_simple_block 作為 block
   - EOF → parse error，丟棄規則（回傳 NULL）

4. **consume_component_value (§5.4.7)**: 元件值
   - `{`/`[`/`(` → consume_simple_block → 包裝為 block CV
   - function token → consume_function → 包裝為 function CV
   - 其他 → clone_token → 包裝為 token CV

5. **consume_simple_block (§5.4.8)**: 簡單區塊
   - 記錄開啟括號類型，找到對應關閉括號
   - 消耗子元件值直到配對的關閉括號或 EOF

6. **consume_function (§5.4.9)**: 函式
   - 名稱來自 function token 的 value
   - 消耗參數值直到 `)` 或 EOF

### 宣告解析（後處理）

- `parse_declarations_from_block()`: 從 `{}` 區塊的元件值中偵測宣告模式
  - 模式: `<ident> <whitespace>* <colon> <whitespace>* <values> <semicolon>`
  - 跳過前導空白和分號
  - 錯誤恢復: 遇到非預期 token 時跳到下一個分號

- `check_important()`: 檢查 `!important`
  - 從值列表末尾反向掃描，跳過空白
  - 尋找 `<ident "important">` + `<delim '!'>`（不分大小寫）
  - 找到時設定 `decl->important = true` 並移除相關 token

### 元件值深拷貝

- `clone_cv()`: 深拷貝元件值（遞迴處理 block 和 function）
- `clone_simple_block()`: 深拷貝區塊及其子值
- `clone_function()`: 深拷貝函式及其參數

### 公開 API

```c
css_stylesheet *css_parse_stylesheet(const char *input, size_t length);
void css_parse_dump(css_stylesheet *sheet, FILE *out);
```

- `css_parse_stylesheet`: 建立 tokenizer → 消耗規則列表 → 清理 → 回傳 stylesheet
- `css_parse_dump`: 增強版傾印，自動偵測 `{}` 區塊中的宣告並格式化輸出

### CLI 模式

```bash
./css_parse tests/basic.css              # 預設模式：解析並傾印 AST（含宣告偵測）
./css_parse --tokens tests/basic.css     # Token 模式：傾印 token 流
```

### 傾印輸出格式

```
STYLESHEET
  QUALIFIED_RULE
    prelude:
      <ident "body">
      <whitespace>
    BLOCK {}
      DECLARATION "color"
        <ident "red">
      DECLARATION "font-size"
        <dimension 16 "px">
  AT_RULE "media"
    prelude:
      <whitespace>
      <ident "screen">
      <whitespace>
    BLOCK {}
      ...
  AT_RULE "import"
    prelude:
      <whitespace>
      FUNCTION "url"
        <string "reset.css">
```

### 記憶體管理

- parser 內部擁有 `current_token`，在 `next_token` 中釋放上一個
- `clone_token` 產生的拷貝由 component_value 擁有
- `parse_declarations_from_block` 中的宣告使用 clone_cv 建立，由呼叫者負責釋放
- `css_parse_stylesheet` 清理 parser 狀態（current_token + tokenizer）

### 測試

```bash
./css_parse tests/basic.css           # body { color: red; font-size: 16px; }
./css_parse tests/tokens_ident.css    # @media, @import, @charset 等
./css_parse tests/parser_basic.css    # 完整測試：多選擇器、!important、函式值、空規則
```

## Task 13: 端到端整合測試

### 測試檔案清單

| 檔案 | 用途 | 測試目標 |
|------|------|---------|
| tests/basic.css | 基本規則 | body { color: red; font-size: 16px; } |
| tests/declarations.css | 宣告測試 | !important, calc(), rgb(), linear-gradient(), 多值 |
| tests/at_rules.css | @規則測試 | @charset, @import, @media, @font-face, @keyframes |
| tests/tokens.css | 完整 token 測試 | 所有 24 種 token 類型 |
| tests/errors.css | 錯誤恢復 | 未結束註解, bad-string, bad-url, 正常解析恢復 |
| tests/tokens_basic.css | 標點測試 | 括號、冒號、分號、逗號、註解 |
| tests/tokens_numeric.css | 數值測試 | 整數、浮點、百分比、dimension、科學記號 |
| tests/tokens_ident.css | Ident 測試 | @media, 選擇器, hash, custom properties |
| tests/tokens_string.css | 字串測試 | 雙引號、單引號、url()、跳脫 |
| tests/parser_basic.css | Parser 測試 | 完整規則、@規則、函式值、!important |
| tests/preprocess_test.css | 前處理測試 | CRLF/CR/FF/NULL 轉換 |
| tests/preprocess_edge.css | 邊界測試 | 前處理邊界情況 |

### Makefile 測試目標

```makefile
test: css_parse          # 基本測試（basic + declarations + at_rules）
test-tokens: css_parse   # Token 傾印測試
test-errors: css_parse   # 錯誤恢復測試（CSSPARSER_PARSE_ERRORS=1）
test-all: test test-tokens test-errors  # 全部測試
```

## Task 14: 記憶體驗證與文件更新

### AddressSanitizer 使用

```bash
cc -std=c11 -Wall -Wextra -pedantic -g -fsanitize=address,undefined \
   -Iinclude src/css_token.c src/css_tokenizer.c src/css_ast.c src/css_parser.c \
   src/css_parse_demo.c -o css_parse_asan

./css_parse_asan tests/basic.css
./css_parse_asan tests/declarations.css
./css_parse_asan tests/at_rules.css
./css_parse_asan --tokens tests/tokens.css
CSSPARSER_PARSE_ERRORS=1 ./css_parse_asan tests/errors.css
```

- **結果**: 零記憶體洩漏、零 buffer overflow、零 use-after-free、零 undefined behavior
- **所有 free 路徑正確**: stylesheet → rules → at_rule/qualified_rule → prelude/block → component_values → tokens

### 專案完成狀態

- **P0 Tokenizer**: 完成 ✓（24 種 token、UTF-8 解碼、前處理、錯誤恢復）
- **P1 Parser**: 完成 ✓（AST 產生、宣告解析、!important、consume-based 演算法）
- **P2 Selectors**: 進行中（CSS Selectors Level 4）
- **P3 Advanced**: 未開始（Nesting、Media Queries、calc()）

## P2a: Selector 資料結構與生命週期 (Tasks 1-3)

### 背景知識

- **CSS Selectors Level 4**: 定義了 CSS 選擇器的語法和語意
- **選擇器層次結構**: selector_list > complex_selector > compound_selector > simple_selector
  - **selector_list**: 逗號分隔的 complex selector 列表（如 `div, .foo`）
  - **complex_selector**: 由 combinator 連接的 compound selector 序列（如 `div > .foo + p`）
  - **compound_selector**: 不含 combinator 的 simple selector 序列（如 `div.foo#bar`）
  - **simple_selector**: 最基本的選擇器單位

- **7 種 simple selector 類型**:
  - `SEL_TYPE` — 元素類型（如 `div`, `p`）
  - `SEL_UNIVERSAL` — 萬用選擇器 `*`
  - `SEL_CLASS` — class 選擇器（如 `.foo`）
  - `SEL_ID` — id 選擇器（如 `#bar`）
  - `SEL_ATTRIBUTE` — 屬性選擇器（如 `[href^="https"]`）
  - `SEL_PSEUDO_CLASS` — 偽類（如 `:hover`）
  - `SEL_PSEUDO_ELEMENT` — 偽元素（如 `::before`）

- **7 種屬性匹配運算子**:
  - `ATTR_EXISTS` — `[attr]` 存在
  - `ATTR_EXACT` — `[attr=val]` 完全匹配
  - `ATTR_INCLUDES` — `[attr~=val]` 空格分隔列表包含
  - `ATTR_DASH` — `[attr|=val]` 完全匹配或前綴加連字號
  - `ATTR_PREFIX` — `[attr^=val]` 開頭匹配
  - `ATTR_SUFFIX` — `[attr$=val]` 結尾匹配
  - `ATTR_SUBSTRING` — `[attr*=val]` 子字串包含

- **4 種 combinator**:
  - `COMB_DESCENDANT` — 空格（後代）
  - `COMB_CHILD` — `>` （子元素）
  - `COMB_NEXT_SIBLING` — `+`（相鄰兄弟）
  - `COMB_SUBSEQUENT_SIBLING` — `~`（一般兄弟）

- **Specificity (a, b, c)**:
  - a: #id 選擇器計數
  - b: .class, [attr], :pseudo-class 計數
  - c: type, ::pseudo-element 計數

### 資料結構

```c
css_simple_selector {
    type, name, attr_match, attr_name, attr_value, attr_case_insensitive
}

css_compound_selector {
    selectors[], count, cap   /* simple selector 動態陣列 */
}

css_complex_selector {
    compounds[], combinators[], count, cap
    /* combinators[i] 位於 compounds[i] 和 compounds[i+1] 之間 */
}

css_selector_list {
    selectors[], count, cap   /* complex selector 動態陣列 */
}
```

### API

```c
/* 生命週期 */
css_simple_selector   *css_simple_selector_create(css_simple_selector_type type);
void                   css_simple_selector_free(css_simple_selector *sel);
css_compound_selector *css_compound_selector_create(void);
void                   css_compound_selector_free(css_compound_selector *comp);
void                   css_compound_selector_append(comp, sel);
css_complex_selector  *css_complex_selector_create(void);
void                   css_complex_selector_free(css_complex_selector *cx);
void                   css_complex_selector_append(cx, comp, comb);
css_selector_list     *css_selector_list_create(void);
void                   css_selector_list_free(css_selector_list *list);
void                   css_selector_list_append(list, cx);

/* 解析（stub） */
css_selector_list *css_parse_selector_list(css_component_value **values, size_t count);

/* Specificity（stub） */
css_specificity css_selector_specificity(css_complex_selector *sel);

/* 傾印 */
void css_selector_dump(css_selector_list *list, FILE *out, int depth);
```

### Dump 輸出格式

```
SELECTOR_LIST (2)
  COMPLEX_SELECTOR
    COMPOUND_SELECTOR
      <type "div">
    COMBINATOR ">"
    COMPOUND_SELECTOR
      <class "foo">
  COMPLEX_SELECTOR
    COMPOUND_SELECTOR
      <id "bar">
```

屬性選擇器格式: `<attribute [href^="https"]>`

### 記憶體管理模式

- 與 css_ast.c 完全一致：calloc 零初始化、strdup 字串擁有權、realloc 動態陣列（cap = cap ? cap*2 : 4）
- `css_complex_selector_append` 中 combinators 陣列與 compounds 陣列同步擴容
- 第一個 compound append 時 comb 參數被忽略（因為沒有前一個 compound）

### 檔案

- `include/css_selector.h` — 所有 enum/struct/API 宣告
- `src/css_selector.c` — 生命週期 + dump + stub
