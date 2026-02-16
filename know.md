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
