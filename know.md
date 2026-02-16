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
