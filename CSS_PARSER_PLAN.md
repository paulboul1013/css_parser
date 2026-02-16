# CSS Parser 實作計畫

以 C11 實作符合 [W3C CSS Syntax Module Level 3](https://www.w3.org/TR/css-syntax-3/) 的 CSS 解析器。
設計風格與本專案 HTML parser 一致：純 C、零外部依賴、兩階段管道。

---

## 一、架構總覽

```
CSS 輸入（字串 / 檔案）
    │
    ▼
┌──────────────────────┐
│  前處理               │  §3.3: CR/FF/CRLF → LF, NULL → U+FFFD
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Tokenizer           │  §4: 消耗 code points，產生 token 流
│  (css_tokenizer.c)   │  24 種 token 類型
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Parser              │  §5: 消耗 token 流，產生 CSS AST
│  (css_parser.c)      │  Stylesheet → Rules → Declarations
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Selector Parser     │  CSS Selectors Level 4
│  (css_selector.c)    │  解析 qualified rule 的 prelude
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Value Parser        │  CSS Values and Units Level 4
│  (css_value.c)       │  解析 declaration 的 value
└──────────┘
```

### 設計原則

| 原則 | 說明 |
|------|------|
| 兩階段管道 | Tokenizer → Parser，與 HTML parser 一致 |
| 容錯優先 | CSS 規範要求 forgiving parsing，無效內容跳過而非中止 |
| 零外部依賴 | 純 C11，僅依賴標準庫 |
| 漸進式實作 | P0 → P3 分階段，每階段可獨立測試 |
| 記憶體安全 | 所有字串 `strdup()`，完整的 `free` 路徑 |

---

## 二、檔案結構

```
css_parser/
├── include/
│   ├── css_token.h          # Token 結構定義
│   ├── css_tokenizer.h      # Tokenizer API
│   ├── css_ast.h            # AST 節點定義（Stylesheet, Rule, Declaration...）
│   ├── css_parser.h         # Parser API
│   ├── css_selector.h       # Selector 解析 API
│   └── css_value.h          # Value 解析 API
├── src/
│   ├── css_token.c          # Token 生命週期（init/free）
│   ├── css_tokenizer.c      # Tokenizer 狀態機
│   ├── css_ast.c            # AST 節點建立/釋放/dump
│   ├── css_parser.c         # 核心 Parser（consume 系列演算法）
│   ├── css_selector.c       # Selector 解析
│   ├── css_value.c          # Value 解析
│   └── css_parse_demo.c     # CLI 入口
├── tests/
│   ├── basic.css
│   ├── selectors.css
│   ├── at_rules.css
│   ├── values.css
│   ├── errors.css
│   └── ...
├── Makefile
└── CSS_PARSER_PLAN.md       # 本文件
```

---

## 三、資料結構

### 3.1 Token（§4.1）

```c
typedef enum {
    CSS_TOKEN_IDENT,           // 識別符（如 color, div）
    CSS_TOKEN_FUNCTION,        // 函式名 + '('（如 rgb(, calc(）
    CSS_TOKEN_AT_KEYWORD,      // @ + 識別符（如 @media）
    CSS_TOKEN_HASH,            // # + ident chars（如 #fff）
    CSS_TOKEN_STRING,          // 引號字串（如 "hello"）
    CSS_TOKEN_BAD_STRING,      // 未結束的字串
    CSS_TOKEN_URL,             // url(...)
    CSS_TOKEN_BAD_URL,         // 格式錯誤的 URL
    CSS_TOKEN_DELIM,           // 單一字元（未被其他規則匹配）
    CSS_TOKEN_NUMBER,          // 數字（42, 3.14）
    CSS_TOKEN_PERCENTAGE,      // 百分比（50%）
    CSS_TOKEN_DIMENSION,       // 帶單位數字（10px, 2em）
    CSS_TOKEN_WHITESPACE,      // 空白
    CSS_TOKEN_CDO,             // <!--
    CSS_TOKEN_CDC,             // -->
    CSS_TOKEN_COLON,           // :
    CSS_TOKEN_SEMICOLON,       // ;
    CSS_TOKEN_COMMA,           // ,
    CSS_TOKEN_OPEN_SQUARE,     // [
    CSS_TOKEN_CLOSE_SQUARE,    // ]
    CSS_TOKEN_OPEN_PAREN,      // (
    CSS_TOKEN_CLOSE_PAREN,     // )
    CSS_TOKEN_OPEN_CURLY,      // {
    CSS_TOKEN_CLOSE_CURLY,     // }
    CSS_TOKEN_EOF              // 輸入結束
} css_token_type;

typedef enum {
    CSS_NUM_INTEGER,           // 整數類型
    CSS_NUM_NUMBER             // 浮點數類型
} css_number_type;

typedef enum {
    CSS_HASH_UNRESTRICTED,     // # 後接非 ident 序列
    CSS_HASH_ID                // # 後接合法 ident 序列
} css_hash_type;

typedef struct {
    css_token_type type;

    // 字串值（IDENT, FUNCTION, AT_KEYWORD, HASH, STRING, URL）
    char *value;

    // 數值（NUMBER, PERCENTAGE, DIMENSION）
    double numeric_value;
    css_number_type number_type;  // integer or number

    // 單位（DIMENSION 專用，如 "px", "em"）
    char *unit;

    // Hash type flag（HASH 專用）
    css_hash_type hash_type;

    // Delim 單字元（DELIM 專用）
    uint32_t delim_codepoint;

    // 位置資訊（除錯用）
    size_t line;
    size_t column;
} css_token;
```

### 3.2 AST 節點（§5.3）

```c
/* ── 節點類型 ── */
typedef enum {
    CSS_NODE_STYLESHEET,       // 頂層樣式表
    CSS_NODE_AT_RULE,          // @-rule（如 @media, @import）
    CSS_NODE_QUALIFIED_RULE,   // 合格規則（選擇器 + 宣告區塊）
    CSS_NODE_DECLARATION,      // 宣告（如 color: red）
    CSS_NODE_COMPONENT_VALUE,  // 組件值（保留的 token / block / function）
    CSS_NODE_SIMPLE_BLOCK,     // 簡單區塊（{}, [], ()）
    CSS_NODE_FUNCTION,         // 函式（如 rgb(255, 0, 0)）
} css_node_type;

/* ── Stylesheet ── */
typedef struct {
    char *location;            // URL（可為 NULL）
    css_rule **rules;          // 規則陣列
    size_t rule_count;
} css_stylesheet;

/* ── At-rule ── */
typedef struct {
    char *name;                // at-keyword 名稱（如 "media"）
    css_component_value **prelude;   // 前言（at-keyword 與 block/; 之間的內容）
    size_t prelude_count;
    css_simple_block *block;   // {} 區塊（可為 NULL，如 @import 以 ; 結尾）
} css_at_rule;

/* ── Qualified rule ── */
typedef struct {
    css_component_value **prelude;   // 前言（選擇器）
    size_t prelude_count;
    css_simple_block *block;   // {} 區塊（宣告區塊）
} css_qualified_rule;

/* ── Declaration ── */
typedef struct {
    char *name;                // 屬性名（如 "color"）
    css_component_value **value;     // 值
    size_t value_count;
    bool important;            // !important 標記
} css_declaration;

/* ── Component value（union 風格） ── */
typedef struct css_component_value {
    css_node_type type;        // COMPONENT_VALUE / SIMPLE_BLOCK / FUNCTION
    union {
        css_token *token;              // 保留的 token
        css_simple_block *block;       // 簡單區塊
        css_function *function;        // 函式
    };
} css_component_value;

/* ── Simple block ── */
typedef struct {
    css_token_type associated_token;   // 開啟 token（{, [, (）
    css_component_value **value;       // 內容
    size_t value_count;
} css_simple_block;

/* ── Function ── */
typedef struct {
    char *name;                // 函式名（如 "rgb"）
    css_component_value **value;     // 參數
    size_t value_count;
} css_function;

/* ── Rule（union wrapper） ── */
typedef struct {
    css_node_type type;        // AT_RULE 或 QUALIFIED_RULE
    union {
        css_at_rule *at_rule;
        css_qualified_rule *qualified_rule;
    };
} css_rule;
```

### 3.3 Selector（CSS Selectors Level 4）

```c
typedef enum {
    SEL_TYPE,                  // 元素類型（div, p）
    SEL_UNIVERSAL,             // *
    SEL_CLASS,                 // .classname
    SEL_ID,                    // #id
    SEL_ATTRIBUTE,             // [attr], [attr=val], [attr~=val], ...
    SEL_PSEUDO_CLASS,          // :hover, :nth-child()
    SEL_PSEUDO_ELEMENT,        // ::before, ::after
} css_simple_selector_type;

typedef enum {
    ATTR_EXISTS,               // [attr]
    ATTR_EXACT,                // [attr=val]
    ATTR_INCLUDES,             // [attr~=val]
    ATTR_DASH_MATCH,           // [attr|=val]
    ATTR_PREFIX,               // [attr^=val]
    ATTR_SUFFIX,               // [attr$=val]
    ATTR_SUBSTRING,            // [attr*=val]
} css_attr_match;

typedef enum {
    COMB_NONE,                 // 無（compound selector 內部）
    COMB_DESCENDANT,           // A B（空白）
    COMB_CHILD,                // A > B
    COMB_NEXT_SIBLING,         // A + B
    COMB_SUBSEQUENT_SIBLING,   // A ~ B
} css_combinator;

/* 簡單選擇器 */
typedef struct {
    css_simple_selector_type type;
    char *name;                // 元素名 / class 名 / id 名 / pseudo 名
    char *ns_prefix;           // 命名空間前綴（可為 NULL）

    // attribute selector 專用
    css_attr_match attr_match;
    char *attr_name;
    char *attr_value;
    bool attr_case_insensitive; // [attr=val i]

    // :nth-child() 等函式型 pseudo-class 的參數
    int nth_a;                 // An+B 的 A
    int nth_b;                 // An+B 的 B
    struct css_selector_list *nth_of;  // :nth-child(An+B of S)
} css_simple_selector;

/* 複合選擇器（compound selector）= 一組無 combinator 連接的簡單選擇器 */
typedef struct {
    css_simple_selector **selectors;
    size_t count;
} css_compound_selector;

/* 複雜選擇器（complex selector）= compound + combinator 鏈 */
typedef struct {
    css_compound_selector **compounds;
    css_combinator *combinators;   // compounds[i] 和 compounds[i+1] 之間的 combinator
    size_t count;                  // compound 數量
} css_complex_selector;

/* 選擇器列表（comma-separated） */
typedef struct css_selector_list {
    css_complex_selector **selectors;
    size_t count;
} css_selector_list;

/* Specificity */
typedef struct {
    unsigned int a;            // ID 選擇器數
    unsigned int b;            // class / attribute / pseudo-class 數
    unsigned int c;            // type / pseudo-element 數
} css_specificity;
```

---

## 四、Tokenizer 規格（§4）

### 4.1 前處理（§3.3）

| 步驟 | 說明 |
|------|------|
| 換行正規化 | CR (U+000D)、FF (U+000C)、CRLF → LF (U+000A) |
| NULL 替換 | U+0000 → U+FFFD |
| Surrogate 替換 | U+D800–U+DFFF → U+FFFD |

與 HTML parser 的 `tokenizer_replace_nulls()` 相同邏輯，可共用。

### 4.2 Code point 分類（§4.2）

| 分類 | 範圍 |
|------|------|
| digit | U+0030–U+0039 (0-9) |
| hex digit | digit + A-F + a-f |
| letter | A-Z + a-z |
| non-ASCII | >= U+0080 |
| ident-start | letter \| non-ASCII \| `_` |
| ident | ident-start \| digit \| `-` |
| non-printable | U+0000–U+0008, U+000B, U+000E–U+001F, U+007F |
| whitespace | LF \| TAB \| SPACE |

### 4.3 Token 消耗主邏輯（§4.3.1）

每次呼叫 `css_tokenizer_next()` 執行以下分派：

```
1. 消耗所有 comment（§4.3.2: /* ... */）
2. 讀取下一個 code point，依其值分派：

   whitespace      → 消耗連續空白，回傳 <whitespace-token>
   U+0022 "        → consume string（ending = "）
   U+0023 #        → 若後接 ident char 或 valid escape → <hash-token>
                      （若後 3 chars 可啟動 ident → type = "id"）
                      否則 → <delim-token> '#'
   U+0027 '        → consume string（ending = '）
   U+0028 (        → <(-token>
   U+0029 )        → <)-token>
   U+002B +        → 若可啟動 number → consume numeric token
                      否則 → <delim-token> '+'
   U+002C ,        → <comma-token>
   U+002D -        → 若可啟動 number → consume numeric token
                      若後兩字為 -> → <CDC-token>
                      若可啟動 ident → consume ident-like token
                      否則 → <delim-token> '-'
   U+002E .        → 若可啟動 number → consume numeric token
                      否則 → <delim-token> '.'
   U+003A :        → <colon-token>
   U+003B ;        → <semicolon-token>
   U+003C <        → 若後三字為 !-- → <CDO-token>
                      否則 → <delim-token> '<'
   U+0040 @        → 若後三字可啟動 ident → consume ident → <at-keyword-token>
                      否則 → <delim-token> '@'
   U+005B [        → <[-token>
   U+005C \        → 若為 valid escape → consume ident-like token
                      否則 → parse error + <delim-token> '\'
   U+005D ]        → <]-token>
   U+007B {        → <{-token>
   U+007D }        → <}-token>
   digit           → reconsume, consume numeric token
   ident-start     → reconsume, consume ident-like token
   EOF             → <EOF-token>
   其他            → <delim-token>（value = 該 code point）
```

### 4.4 子演算法

| 演算法 | §節 | 說明 |
|--------|-----|------|
| Consume comments | §4.3.2 | `/* ... */`，未結束 → parse error |
| Consume numeric token | §4.3.3 | number → dimension / percentage / number |
| Consume ident-like token | §4.3.4 | ident → ident / function / url |
| Consume string token | §4.3.5 | 引號字串，newline → bad-string |
| Consume URL token | §4.3.6 | 非引號 URL，特殊字元 → bad-url |
| Consume escaped code point | §4.3.7 | `\` 後的 hex escape 或字元 escape |
| Valid escape check | §4.3.8 | `\` + 非 newline |
| Start ident sequence check | §4.3.9 | 前瞻 3 字元判斷 |
| Start number check | §4.3.10 | 前瞻 3 字元判斷 |
| Consume ident sequence | §4.3.11 | 累積 ident chars + escapes |
| Consume number | §4.3.12 | 整數 / 小數 / 科學記號 |
| Convert string to number | §4.3.13 | s × (i + f×10^-d) × 10^(t×e) |
| Consume bad URL remnants | §4.3.14 | 跳到 `)` 或 EOF |

### 4.5 Tokenizer 結構

```c
typedef struct {
    const char *input;         // 前處理後的 UTF-8 輸入
    size_t length;             // 輸入長度
    size_t pos;                // 當前讀取位置（byte offset）

    // Unicode 解碼狀態
    uint32_t current;          // 當前 code point
    uint32_t peek1;            // 前瞻 1
    uint32_t peek2;            // 前瞻 2
    uint32_t peek3;            // 前瞻 3（某些判斷需要）

    // 行列追蹤（錯誤報告用）
    size_t line;
    size_t column;

    // 回傳的 token（呼叫端消耗後須 free）
    css_token current_token;
} css_tokenizer;
```

---

## 五、Parser 規格（§5）

### 5.1 入口點（§5.3）

| 入口 | §節 | 用途 |
|------|-----|------|
| Parse a stylesheet | §5.3.3 | 解析完整 CSS 檔案 → `css_stylesheet` |
| Parse a list of rules | §5.3.4 | `<style>` 標籤內容 |
| Parse a rule | §5.3.5 | 單一規則 |
| Parse a declaration | §5.3.6 | 單一宣告（`style=""` 屬性） |
| Parse a style block's contents | §5.3.7 | `{ }` 區塊內容（含 CSS Nesting） |
| Parse a list of declarations | §5.3.8 | `style=""` 屬性 / `@font-face` 內容 |
| Parse a component value | §5.3.9 | 單一 component value |
| Parse a list of component values | §5.3.10 | 任意 token 序列 |
| Parse a comma-separated list | §5.3.11 | 逗號分隔列表 |

### 5.2 Consume 演算法（§5.4）

| 演算法 | §節 | 產出 |
|--------|-----|------|
| Consume a list of rules | §5.4.1 | `css_rule[]`（接受 top-level flag） |
| Consume an at-rule | §5.4.2 | `css_at_rule` |
| Consume a qualified rule | §5.4.3 | `css_qualified_rule` 或 NULL |
| Consume a style block's contents | §5.4.4 | `css_declaration[]` + `css_rule[]`（CSS Nesting） |
| Consume a list of declarations | §5.4.5 | `css_declaration[]`（可含 at-rules） |
| Consume a declaration | §5.4.6 | `css_declaration` 或 NULL |
| Consume a component value | §5.4.7 | `css_component_value`（token / block / function） |
| Consume a simple block | §5.4.8 | `css_simple_block`（匹配 `{}`/`[]`/`()`） |
| Consume a function | §5.4.9 | `css_function` |

### 5.3 核心解析流程

#### Consume a list of rules（§5.4.1）

```
輸入：top-level flag（boolean）
輸出：規則列表

重複消耗下一個 token：
  whitespace    → 忽略
  EOF           → 回傳規則列表
  CDO / CDC     → top-level? 忽略 : reconsume → consume qualified rule
  at-keyword    → reconsume → consume at-rule → 加入列表
  其他          → reconsume → consume qualified rule → 非 NULL 則加入列表
```

#### Consume an at-rule（§5.4.2）

```
建立 at-rule（name = at-keyword 值）

重複消耗下一個 token：
  ;             → 回傳 at-rule
  EOF           → parse error，回傳 at-rule
  {             → consume simple block → 設為 at-rule 的 block → 回傳
  其他          → reconsume → consume component value → 加入 prelude
```

#### Consume a qualified rule（§5.4.3）

```
建立 qualified rule

重複消耗下一個 token：
  EOF           → parse error，回傳 NULL（丟棄）
  {             → consume simple block → 設為 block → 回傳
  其他          → reconsume → consume component value → 加入 prelude
```

#### Consume a declaration（§5.4.6）

```
前提：下一個 token 已確認為 ident-token

1. 消耗 ident-token → name
2. 跳過 whitespace
3. 若下一個非 colon → parse error，回傳 NULL
4. 消耗 colon，跳過 whitespace
5. 消耗 component values 直到 EOF → value 列表
6. 檢查最後兩個非空白 component value 是否為 '!' + 'important'
   → 是的話移除它們，設 important = true
7. 移除尾部 whitespace
8. 回傳 declaration
```

#### Consume a component value（§5.4.7）

```
消耗下一個 token：
  { / [ / (     → consume simple block → 回傳
  function      → consume function → 回傳
  其他          → 回傳該 token
```

#### Consume a simple block（§5.4.8）

```
前提：當前 token 為 { / [ / (
記錄 mirror token（{ → }, [ → ], ( → )）

重複消耗下一個 token：
  mirror token  → 回傳 block
  EOF           → parse error，回傳 block
  其他          → reconsume → consume component value → 加入 block
```

#### Consume a function（§5.4.9）

```
前提：當前 token 為 function-token
建立 function（name = token 值）

重複消耗下一個 token：
  )             → 回傳 function
  EOF           → parse error，回傳 function
  其他          → reconsume → consume component value → 加入 function
```

---

## 六、Selector 解析（CSS Selectors Level 4）

### 6.1 語法結構

```
<selector-list>    = <complex-selector> [',' <complex-selector>]*
<complex-selector> = <compound-selector> [<combinator> <compound-selector>]*
<compound-selector> = [<type-selector>]? <subclass-selector>* <pseudo-element>*
<subclass-selector> = <id-selector> | <class-selector> | <attribute-selector> | <pseudo-class>
```

### 6.2 簡單選擇器

| 類型 | 語法 | 實作方式 |
|------|------|---------|
| Type selector | `div`, `p` | ident-token |
| Universal | `*` | delim-token `*` |
| Class | `.foo` | delim-token `.` + ident-token |
| ID | `#bar` | hash-token（type = id） |
| Attribute | `[attr op val]` | `[` 起始的 simple block 解析 |
| Pseudo-class | `:hover` | colon + ident |
| Pseudo-class (fn) | `:nth-child(2n+1)` | colon + function-token |
| Pseudo-element | `::before` | colon + colon + ident |

### 6.3 Combinator

| 語法 | 名稱 | 辨識方式 |
|------|------|---------|
| 空白 | Descendant | whitespace-token（在兩個 compound 之間） |
| `>` | Child | delim-token `>` |
| `+` | Next-sibling | delim-token `+` |
| `~` | Subsequent-sibling | delim-token `~` |

### 6.4 An+B 語法

`:nth-child()`、`:nth-of-type()` 等使用 An+B 微語法：

```
<an+b> = odd | even | <integer>
       | <n-dimension>
       | '+' <n-dimension>
       | '-' <n-dimension>
       | <ndash-dimension> <signless-integer>
       | '+' <ndash-dimension> <signless-integer>
       | '-' <ndash-dimension> <signless-integer>
       | <ndashdigit-dimension>
       | '+' <ndashdigit-dimension>
       | '-' <ndashdigit-dimension>
       | <dashndash-ident> <signless-integer>
       | '-n' <signless-integer>
       | '-n-' <signless-integer>
       | 'n'
       | '+n'
       | '-n'
       | 'n-' <signless-integer>
       | '+n-' <signless-integer>
       | <n-dimension> '+' <signless-integer>
       | <n-dimension> '-' <signless-integer>
```

### 6.5 Specificity 計算

```c
css_specificity calc_specificity(css_complex_selector *sel) {
    // a = count of #id selectors
    // b = count of .class, [attr], :pseudo-class (except :where())
    // c = count of type selectors, ::pseudo-elements
    // :is(), :not(), :has() → 取最高 specificity 的參數
    // :where() → (0, 0, 0)
}
```

### 6.6 Forgiving Selector Parsing

`:is()`、`:where()`、`:has()` 使用 forgiving 解析：

- 個別無效選擇器被丟棄，不影響整體列表
- 一般選擇器列表：一個無效 → 整體無效

---

## 七、@-rule 處理

### 7.1 Statement @-rules（以 `;` 結尾，無 block）

| @-rule | 語法 | prelude 內容 |
|--------|------|-------------|
| `@charset` | `@charset "name";` | 非真正 at-rule，byte-level 偵測（§3.2） |
| `@import` | `@import url [layer] [supports()] [media];` | URL + 可選修飾 |
| `@namespace` | `@namespace [prefix] url;` | 前綴 + URL |

### 7.2 Block @-rules（含 `{ }` block）

| @-rule | block 內容 | 解析方式 |
|--------|-----------|---------|
| `@media` | 巢套規則列表 | prelude → media query list，block → consume list of rules |
| `@supports` | 巢套規則列表 | prelude → supports condition，block → consume list of rules |
| `@font-face` | 宣告列表 | 無 prelude，block → consume list of declarations |
| `@keyframes` | keyframe 規則列表 | prelude → animation name，block → keyframe rules |
| `@layer` | 巢套規則列表（或無 block） | block form → rules，statement form → 以 `;` 結尾 |
| `@page` | 宣告列表 | prelude → page selector，block → declarations |
| `@container` | 巢套規則列表 | prelude → container query，block → rules |

### 7.3 Media Query 解析

```
<media-query-list>  = <media-query> [',' <media-query>]*
<media-query>       = <media-condition>
                    | [not | only]? <media-type> [and <media-condition-without-or>]?
<media-condition>   = <media-not> | <media-in-parens> [<media-and>* | <media-or>*]
<media-in-parens>   = '(' <media-condition> ')' | <media-feature> | <general-enclosed>
<media-feature>     = '(' [<mf-plain> | <mf-boolean> | <mf-range>] ')'

三種 media type：all, print, screen
已淘汰 type（tty, tv, projection...）→ 識別但不匹配

Range 語法：(width >= 600px), (400px < width < 1000px)
Boolean 語法：(color) → color 非 0/none 即為 true

錯誤回復：語法錯誤的 media query → 替換為 "not all"（永不匹配）
```

---

## 八、Value 解析（CSS Values and Units Level 4）

### 8.1 基本值類型

| 類型 | Token 來源 | 範例 |
|------|-----------|------|
| `<integer>` | number-token（integer type） | `42` |
| `<number>` | number-token | `3.14` |
| `<length>` | dimension-token | `10px`, `2em`, `100vw` |
| `<percentage>` | percentage-token | `50%` |
| `<angle>` | dimension-token（deg/grad/rad/turn） | `45deg` |
| `<time>` | dimension-token（s/ms） | `300ms` |
| `<frequency>` | dimension-token（Hz/kHz） | `440Hz` |
| `<resolution>` | dimension-token（dpi/dpcm/dppx） | `2dppx` |
| `<ratio>` | number / number | `16/9` |
| `<string>` | string-token | `"hello"` |
| `<url>` | url-token 或 function("url") + string | `url(img.png)` |
| `<ident>` / `<custom-ident>` | ident-token | `auto`, `none` |
| `<dashed-ident>` | ident-token（`--` 開頭） | `--main-color` |
| `<color>` | 多種 | `red`, `#fff`, `rgb()`, `hsl()` |

### 8.2 數學函式

| 函式 | 說明 |
|------|------|
| `calc()` | 四則運算，混合單位（如 `calc(100% - 20px)`） |
| `min()` / `max()` | 比較函式 |
| `clamp()` | `clamp(min, val, max)` |
| `round()` / `mod()` / `rem()` | 步進值函式 |
| `sin()` / `cos()` / `tan()` | 三角函式 |
| `abs()` / `sign()` | 符號函式 |

常數：`e`, `pi`, `infinity`, `-infinity`, `NaN`

### 8.3 CSS-Wide Keywords

所有屬性都接受：`initial`, `inherit`, `unset`, `revert`, `revert-layer`

---

## 九、錯誤處理

CSS 的錯誤處理哲學：**forgiving by design**（容錯設計）。

### 9.1 錯誤回復策略

| 情境 | 行為 |
|------|------|
| 未結束的 comment（EOF before `*/`） | parse error，視為已結束 |
| 未結束的 string（EOF before 引號） | parse error，回傳 string-token |
| String 中遇到 newline | parse error，回傳 bad-string-token |
| 未結束的 URL（EOF before `)`） | parse error，回傳 url-token |
| URL 中遇到非法字元 | parse error，回傳 bad-url-token |
| 獨立的 `\`（後接 newline） | parse error，回傳 delim-token |
| 未結束的 block / function | parse error，視為已結束 |
| Declaration 缺少 `:` | parse error，丟棄整個 declaration |
| Qualified rule 缺少 `{ }` | parse error，丟棄整個 rule |
| 宣告列表中遇到非預期 token | parse error，跳到下一個 `;` 或 EOF |
| Media query 語法錯誤 | 替換為 `not all`（永不匹配） |
| Selector 列表中有無效選擇器 | 整個選擇器列表失效（除 forgiving 模式） |

### 9.2 錯誤報告

與 HTML parser 類似，使用環境變數控制：

```c
// CSSPARSER_PARSE_ERRORS=1 啟用 parse error 報告到 stderr
static void css_parse_error(const char *msg, size_t line, size_t col) {
    if (getenv("CSSPARSER_PARSE_ERRORS"))
        fprintf(stderr, "CSS parse error at %zu:%zu: %s\n", line, col, msg);
}
```

---

## 十、實作優先級

### P0 — 核心 Tokenizer（必須首先完成）

| 功能 | 說明 |
|------|------|
| 前處理（CR/LF/NULL） | §3.3 |
| 24 種 token 類型定義 | §4.1 |
| Token 消耗主邏輯 | §4.3.1 |
| Comment 消耗 | §4.3.2 |
| Numeric token 消耗 | §4.3.3 |
| Ident-like token 消耗 | §4.3.4 |
| String token 消耗 | §4.3.5 |
| URL token 消耗 | §4.3.6 |
| Escaped code point | §4.3.7 |
| 5 個 helper（valid escape, start ident, start number, ident seq, number） | §4.3.8–§4.3.14 |

**驗證**：能將任意 CSS 輸入正確切分為 token 流。

### P1 — 核心 Parser

| 功能 | 說明 |
|------|------|
| AST 資料結構定義 | §5 |
| Parse a stylesheet | §5.3.3 |
| Consume a list of rules | §5.4.1 |
| Consume an at-rule | §5.4.2 |
| Consume a qualified rule | §5.4.3 |
| Consume a list of declarations | §5.4.5 |
| Consume a declaration | §5.4.6 |
| Consume a component value | §5.4.7 |
| Consume a simple block | §5.4.8 |
| Consume a function | §5.4.9 |
| `!important` 偵測 | §5.4.6 步驟 6 |
| AST dump（ASCII 樹輸出） | 除錯用 |
| 記憶體管理（完整 free 路徑） | |

**驗證**：能解析一般 CSS 檔案為正確的 AST，dump 輸出可驗證結構。

### P2 — Selector 解析

| 功能 | 說明 |
|------|------|
| Type / Universal selector | `div`, `*` |
| Class / ID selector | `.foo`, `#bar` |
| Attribute selector（7 種匹配） | `[attr]`, `[attr=val]`, `[attr^=val]`... |
| Pseudo-class（keyword 型） | `:hover`, `:focus`, `:first-child` |
| Pseudo-class（function 型） | `:nth-child()`, `:not()`, `:is()` |
| Pseudo-element | `::before`, `::after` |
| An+B 微語法解析 | `2n+1`, `odd`, `even` |
| Combinator（4 種） | 空白, `>`, `+`, `~` |
| Compound → Complex → List 組裝 | 完整選擇器結構 |
| Specificity 計算 | (a, b, c) 三元組 |
| Forgiving selector parsing | `:is()`, `:where()`, `:has()` |
| Namespace prefix | `ns|element` |

**驗證**：能解析各種選擇器並計算 specificity。

### P3 — 進階功能

| 功能 | 說明 |
|------|------|
| Parse a style block's contents | §5.4.4（含 CSS Nesting） |
| Media Query 解析 | `@media` prelude → condition tree |
| `@supports` condition 解析 | feature/selector 測試 |
| `@import` URL + 修飾解析 | layer, supports, media |
| `@keyframes` rule 解析 | from/to/percentage selectors |
| `@font-face` descriptor 解析 | font descriptors |
| `@layer` 解析 | block / statement 兩種形式 |
| `@container` 解析 | container query |
| Value type 驗證 | `<length>`, `<color>`, `<percentage>` 等 |
| `calc()` 表達式解析 | 四則運算 + 單位推導 |
| CSS custom properties | `--var-name: value` |
| `var()` 函式替換 | `var(--name, fallback)` |
| CSS serialization | AST → CSS 字串 |

---

## 十一、測試策略

### 11.1 測試方式

與 HTML parser 一致：測試為 `.css` 檔案，解析後輸出 AST dump，手動比對。

```sh
make css_parse
./css_parse tests/basic.css           # dump AST
CSSPARSER_PARSE_ERRORS=1 ./css_parse tests/errors.css  # 含 parse error 報告
```

### 11.2 測試案例規劃

| 測試檔 | 涵蓋功能 |
|--------|---------|
| `basic.css` | 基本規則：type selector + 常見屬性 |
| `selectors.css` | 全部選擇器類型 + combinator |
| `selectors_nth.css` | An+B 語法 |
| `at_rules.css` | @media, @import, @font-face, @keyframes |
| `at_supports.css` | @supports conditions |
| `at_layer.css` | @layer block + statement |
| `values.css` | 各種值類型（length, color, percentage...） |
| `values_calc.css` | calc() 表達式 |
| `strings.css` | 引號字串、escape、多行字串 |
| `urls.css` | url() token、引號 URL、bad URL |
| `comments.css` | 註解（含未結束） |
| `errors.css` | 各種 parse error 回復 |
| `nesting.css` | CSS Nesting（& selector） |
| `important.css` | !important 偵測 |
| `custom_props.css` | CSS custom properties + var() |
| `specificity.css` | specificity 計算驗證 |
| `whitespace.css` | 空白處理 edge cases |
| `escape.css` | Unicode escape + 字元 escape |
| `encoding.css` | @charset + BOM 偵測 |
| `media_queries.css` | 完整 media query 語法 |

### 11.3 AST Dump 格式

```
STYLESHEET
├── QUALIFIED_RULE
│   ├── prelude: "body"
│   └── BLOCK {}
│       ├── DECLARATION "color"
│       │   └── value: <ident-token "red">
│       └── DECLARATION "font-size"
│           └── value: <dimension-token 16 "px">
├── AT_RULE "@media"
│   ├── prelude: <ident-token "screen"> <whitespace> ...
│   └── BLOCK {}
│       └── QUALIFIED_RULE
│           ├── prelude: ".container"
│           └── BLOCK {}
│               └── DECLARATION "max-width"
│                   └── value: <dimension-token 1200 "px">
```

---

## 十二、與 HTML Parser 的整合點

### 12.1 共用模組

| 模組 | 共用方式 |
|------|---------|
| 前處理（CR/LF/NULL） | 可共用 `tokenizer_replace_nulls()` 邏輯 |
| Encoding sniffing | CSS 使用 @charset / BOM / 環境編碼，可複用 `encoding.c` |
| 記憶體管理模式 | 同 HTML parser 的 `strdup()` + free 模式 |

### 12.2 互動方式

```
HTML Parser
    │
    ├── <style> 標籤內容 → CSS Parser（parse a list of rules）
    ├── style="" 屬性 → CSS Parser（parse a list of declarations）
    └── <link rel="stylesheet"> → CSS Parser（parse a stylesheet）
```

### 12.3 日後擴展

- CSS 與 HTML parser 組合後，可實作：
  - Selector matching（比對 HTML 節點與 CSS 選擇器）
  - Computed style 計算（Cascade + Inheritance）
  - 簡易 layout engine

---

## 十三、參考規範

| 規範 | URL |
|------|-----|
| CSS Syntax Module Level 3 | https://www.w3.org/TR/css-syntax-3/ |
| CSS Selectors Level 4 | https://www.w3.org/TR/selectors-4/ |
| CSS Values and Units Level 4 | https://www.w3.org/TR/css-values-4/ |
| CSS Cascade Level 5 | https://www.w3.org/TR/css-cascade-5/ |
| CSS Conditional Rules Level 3 | https://www.w3.org/TR/css-conditional-3/ |
| Media Queries Level 5 | https://www.w3.org/TR/mediaqueries-5/ |
| CSS Nesting | https://www.w3.org/TR/css-nesting-1/ |
| CSS Color Level 4 | https://www.w3.org/TR/css-color-4/ |

---

## 十四、預估規模

| 模組 | 預估行數 |
|------|---------|
| css_token.h/c | ~200 |
| css_tokenizer.h/c | ~800–1,000 |
| css_ast.h/c | ~400 |
| css_parser.h/c | ~600–800 |
| css_selector.h/c | ~600–800 |
| css_value.h/c | ~400–600 |
| **總計** | **~3,000–3,800** |

與 HTML parser 的 ~8,800 行相比，CSS parser 規模較小（CSS tokenizer 比 HTML tokenizer 簡單很多，且無 tree construction 的複雜狀態管理）。
