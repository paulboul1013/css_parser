# P2a — CSS Selector 核心解析器設計

日期：2026-02-18

## 範圍

實作 CSS Selectors Level 4 的核心子集，足以支撐 render tree 整合。

### 包含
- Type / Universal selector（`div`, `*`）
- Class / ID selector（`.foo`, `#bar`）
- Attribute selector（7 種匹配：`[attr]`, `[attr=val]`, `[attr~=val]`, `[attr|=val]`, `[attr^=val]`, `[attr$=val]`, `[attr*=val]`）
- Pseudo-class（keyword 型：`:hover`, `:focus`, `:first-child` 等）
- Pseudo-element（`::before`, `::after`）
- Combinator（4 種：descendant, child, next-sibling, subsequent-sibling）
- Compound → Complex → List 組裝
- Specificity 計算（a, b, c 三元組）

### 不包含（留給 P2b）
- An+B 微語法（`:nth-child()` 等函式型 pseudo-class 參數解析）
- Forgiving selector parsing（`:is()`, `:where()`, `:has()`）
- Namespace prefix（`ns|element`）

## 資料結構

### 新增型別（`include/css_selector.h`）

```c
typedef enum {
    SEL_TYPE,           // div, p
    SEL_UNIVERSAL,      // *
    SEL_CLASS,          // .classname
    SEL_ID,             // #id
    SEL_ATTRIBUTE,      // [attr], [attr=val]
    SEL_PSEUDO_CLASS,   // :hover, :focus
    SEL_PSEUDO_ELEMENT, // ::before, ::after
} css_simple_selector_type;

typedef enum {
    ATTR_EXISTS,        // [attr]
    ATTR_EXACT,         // [attr=val]
    ATTR_INCLUDES,      // [attr~=val]
    ATTR_DASH_MATCH,    // [attr|=val]
    ATTR_PREFIX,        // [attr^=val]
    ATTR_SUFFIX,        // [attr$=val]
    ATTR_SUBSTRING,     // [attr*=val]
} css_attr_match;

typedef enum {
    COMB_DESCENDANT,         // A B（空白）
    COMB_CHILD,              // A > B
    COMB_NEXT_SIBLING,       // A + B
    COMB_SUBSEQUENT_SIBLING, // A ~ B
} css_combinator;

typedef struct {
    css_simple_selector_type type;
    char *name;                     // 元素名 / class 名 / id 名 / pseudo 名

    // attribute selector 專用
    css_attr_match attr_match;
    char *attr_name;
    char *attr_value;
    bool attr_case_insensitive;     // [attr=val i]
} css_simple_selector;

typedef struct {
    css_simple_selector **selectors;
    size_t count;
    size_t cap;
} css_compound_selector;

typedef struct {
    css_compound_selector **compounds;
    css_combinator *combinators;    // compounds[i] → compounds[i+1] 之間
    size_t count;                   // compound 數量
    size_t cap;
} css_complex_selector;

typedef struct css_selector_list {
    css_complex_selector **selectors;
    size_t count;
    size_t cap;
} css_selector_list;

typedef struct {
    unsigned int a;     // ID selectors
    unsigned int b;     // class + attribute + pseudo-class
    unsigned int c;     // type + pseudo-element
} css_specificity;
```

### 修改現有結構

`css_qualified_rule`（`include/css_ast.h`）新增：
```c
css_selector_list *selectors;  // 解析後的選擇器（可為 NULL）
```

## 解析演算法

### 輸入來源

直接讀取 `qualified_rule->prelude`（`css_component_value` 陣列），不需重新 tokenize。

### 解析流程

```
css_selector_list *css_parse_selector_list(cv[], count):
  以逗號 token 為分隔，切分為 complex selectors
  任一 complex selector 解析失敗 → 整個列表無效（回傳 NULL）

css_complex_selector *parse_complex_selector(cv[], start, end):
  迴圈:
    1. 跳過前導空白
    2. parse_compound_selector()
    3. 檢查後續:
       - delim '>' / '+' / '~' → 記錄 combinator，繼續
       - whitespace → peek 下一個非空白 token
         - 是 compound 開頭 → descendant combinator
         - 是結尾/逗號 → 結束
       - 結尾 → 結束

css_compound_selector *parse_compound_selector(cv[], *pos):
  1. 嘗試 type selector: ident → SEL_TYPE, delim '*' → SEL_UNIVERSAL
  2. 迴圈 subclass selectors:
     - hash token (id type) → SEL_ID
     - delim '.' + ident → SEL_CLASS
     - simple_block '[...]' → parse_attribute_selector()
     - colon + ident → SEL_PSEUDO_CLASS
     - colon + colon + ident → SEL_PSEUDO_ELEMENT
  3. 至少 1 個 selector，否則錯誤

parse_attribute_selector(block_values):
  讀取: attr_name [op attr_value [case_flag]]
  op 辨識: '=' / '~=' / '|=' / '^=' / '$=' / '*='
```

### Specificity 計算

```c
css_specificity css_selector_specificity(css_complex_selector *sel):
  走訪所有 compound 的所有 simple selector:
    SEL_ID         → a++
    SEL_CLASS      → b++
    SEL_ATTRIBUTE  → b++
    SEL_PSEUDO_CLASS → b++
    SEL_TYPE       → c++
    SEL_PSEUDO_ELEMENT → c++
    SEL_UNIVERSAL  → 不計
```

### 整合點

在 `css_parser.c` 的 `css_parse_stylesheet()` 中，建立完 qualified rule 後，自動呼叫 `css_parse_selector_list()` 解析 prelude，結果存入 `qr->selectors`。

## 檔案清單

| 檔案 | 動作 |
|------|------|
| `include/css_selector.h` | 新增 — 所有 selector 型別 + API |
| `src/css_selector.c` | 新增 — selector 解析 + specificity |
| `include/css_ast.h` | 修改 — qualified_rule 新增 selectors 欄位 |
| `src/css_ast.c` | 修改 — qualified_rule free/dump 處理 selectors |
| `src/css_parser.c` | 修改 — 建立 qr 後呼叫 selector 解析 |
| `src/css_parse_demo.c` | 修改 — dump 輸出顯示 selector 結構 |
| `Makefile` | 修改 — 加入 css_selector.c |
| `tests/selectors.css` | 新增 — 選擇器測試用例 |

## 錯誤處理

- 無法辨識的 token → 整個 complex selector 失敗
- 空 selector list → 回傳 NULL，qualified rule 的 selectors 欄位為 NULL
- CSS 規範：一個無效選擇器使整個列表無效（非 forgiving 模式）
