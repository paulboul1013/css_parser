# CSS Parser 實作進度

## 已完成

- [x] Task 1: 建立專案骨架與 Makefile
  - 目錄結構 (include/, src/, tests/)
  - Makefile (匹配 html_parser 風格)
  - 標頭檔骨架 (css_token.h, css_tokenizer.h, css_ast.h, css_parser.h)
  - 原始碼骨架 (css_token.c, css_tokenizer.c, css_ast.c, css_parser.c)
  - CLI 入口點 (css_parse_demo.c — 讀檔並印出位元組數)
  - 測試檔案 (tests/basic.css)
  - 編譯零警告驗證通過

- [x] Task 2: Token 結構與生命週期
  - 24 種 token 類型 (css_token_type 列舉)
  - css_token 結構 (value, numeric_value, unit, hash_type, delim_codepoint, line, column)
  - css_token_create() / css_token_free() / css_token_type_name()
  - css_number_type / css_hash_type 列舉

- [x] Task 3: Tokenizer 前處理與 UTF-8 解碼
  - css_tokenizer 結構 (input, length, pos, current/peek1/peek2/peek3, line, column, reconsume)
  - 前處理 (CRLF→LF, CR→LF, FF→LF, NULL→U+FFFD)
  - UTF-8 解碼器 (1~4 位元組, overlong 檢查, 無效序列→U+FFFD)
  - 4 格先行查看管線 (fill_lookahead, consume_codepoint)
  - css_tokenizer_create() / css_tokenizer_next() (暫時只回傳 EOF) / css_tokenizer_free()
  - CLI --tokens 模式 (css_parse_demo.c)
  - 測試檔案 (tests/preprocess_test.css, tests/preprocess_edge.css)
  - 編譯零警告驗證通過

## 未完成

- [x] Task 4: 空白/標點/Comment token
  - Code point 分類輔助函式 (is_whitespace, is_digit, is_letter 等)
  - 註解消耗 (consume_comments, CSS Syntax §4.3.2)
  - 解析錯誤輔助函式 (css_parse_error, CSSPARSER_PARSE_ERRORS 環境變數)
  - Token 分發主迴圈 (空白、標點、delim fallback)
  - make_token 輔助函式
  - 測試檔案 (tests/tokens_basic.css)
  - 編譯零警告驗證通過
- [x] Task 5: 數值 token (number/percentage/dimension)
  - 檢查輔助函式 (valid_escape, starts_number, starts_ident_sequence)
  - UTF-8 編碼函式 (encode_utf8)
  - 跳脫 code point 消耗 (consume_escaped_codepoint)
  - 數字消耗 (consume_number: 整數/浮點/科學記號)
  - Ident 序列消耗 (consume_ident_sequence: dimension 單位用)
  - 數值 token 消耗 (consume_numeric_token: number/percentage/dimension)
  - 分發邏輯更新 (digit/+/-/. 開頭的數字)
  - --tokens 增強輸出 (number/percentage/dimension/delim 顯示值)
  - _POSIX_C_SOURCE 200809L 定義（strdup 支援）
  - 測試檔案 (tests/tokens_numeric.css)
  - 編譯零警告驗證通過
- [x] Task 6: Ident/Function/Hash/At token
  - consume_ident_like_token (ident, function, url 分派)
  - Hash token (#xxx → id/unrestricted)
  - At-keyword token (@media, @import 等)
  - CDC (-->) 和 CDO (<!--) token
  - 反斜線跳脫開始 ident
  - --tokens 增強輸出 (ident, function, at-keyword, hash)
  - 測試檔案 (tests/tokens_ident.css)
  - 編譯零警告驗證通過
- [x] Task 7: String 和 URL token
  - consume_string_token (§4.3.5: 雙引號/單引號字串, 跳脫, bad-string)
  - consume_url_token (§4.3.6: 不帶引號的 URL, bad-url)
  - consume_bad_url_remnants (§4.3.14: 消耗 bad URL 剩餘字元)
  - 主分發新增 '"' 和 '\'' 觸發 string token
  - consume_ident_like_token 更新: unquoted URL 呼叫 consume_url_token
  - 移除 (void)is_non_printable 抑制（現已被 consume_url_token 使用）
  - --tokens 增強輸出 (string, bad-string, url, bad-url)
  - 測試檔案 (tests/tokens_string.css)
  - 編譯零警告驗證通過
- [x] Task 8: Tokenizer 完整性驗證
- [x] Task 9: AST 結構定義
  - css_node_type 列舉（7 種節點類型）
  - 完整結構定義（stylesheet, rule, at_rule, qualified_rule, declaration, simple_block, function, component_value）
  - 建立函式（11 個 create 函式，calloc + strdup）
  - 釋放函式（8 個 free 函式，NULL 安全，遞迴釋放）
  - 附加輔助函式（6 個 append 函式，realloc 動態陣列）
  - 傾印函式（css_ast_dump，縮排式 AST 輸出）
  - 單元測試（tests/test_ast.c，12 個測試案例）
  - 編譯零警告驗證通過
- [x] Task 10: Parser 骨架
  - css_parser_ctx 內部結構（tokenizer, current_token, reconsume）
  - next_token / reconsume token 消耗機制
  - clone_token token 深拷貝輔助函式
  - consume_component_value (§5.4.7)
  - consume_simple_block (§5.4.8)
  - consume_function (§5.4.9)
  - consume_list_of_rules (§5.4.1)
  - css_parse_stylesheet 公開 API
  - css_parser.h 標頭檔
  - 編譯零警告驗證通過
- [x] Task 11: At-rule 和 Qualified rule
  - consume_at_rule (§5.4.2: prelude + block/semicolon)
  - consume_qualified_rule (§5.4.3: prelude + block, EOF 丟棄)
  - CDO/CDC 處理（top_level 跳過）
  - css_parse_dump 增強傾印函式
  - 測試: @media, @import, @charset, qualified rules
  - 編譯零警告驗證通過
- [x] Task 12: Declaration 與 !important
  - parse_declarations_from_block 後處理宣告解析
  - check_important !important 偵測（不分大小寫）
  - clone_cv / clone_simple_block / clone_function 深拷貝
  - cv_is_token 輔助函式
  - 錯誤恢復（跳到下一個分號）
  - css_parse_demo.c 整合（預設模式使用 parser）
  - 測試檔案 (tests/parser_basic.css)
  - 編譯零警告驗證通過
- [x] Task 13: 端到端整合測試
  - 新增測試檔案 (tests/at_rules.css, tests/declarations.css)
  - Makefile 新增 test/test-tokens/test-errors/test-all 目標
  - 驗證: basic.css, declarations.css, at_rules.css 解析正確
  - 驗證: tokens.css token 傾印正確
  - 驗證: errors.css 錯誤恢復正常（CSSPARSER_PARSE_ERRORS=1）
  - 編譯零警告驗證通過
- [x] Task 14: 記憶體驗證與文件更新
  - AddressSanitizer 驗證: 零記憶體錯誤（所有測試檔案）
  - UndefinedBehaviorSanitizer 驗證: 零未定義行為
  - CLAUDE.md 更新: 建構命令、檔案清單、實作狀態
  - know.md 更新: 完整背景知識
  - list.md 更新: 完整進度追蹤

## P0 + P1 原型完成

所有 14 個任務已完成。Tokenizer（24 種 token）和 Parser（AST 產生）均通過完整測試和記憶體安全驗證。

---

## P2: Selector 解析

### 已完成

- [x] P2a Task 1-3: Selector 資料結構、生命週期、dump
  - include/css_selector.h 標頭檔（7 種 simple selector type、7 種 attr match、4 種 combinator、specificity、所有 struct/API 宣告）
  - src/css_selector.c 生命週期函式（create/free/append，遵循 calloc/realloc 模式）
  - css_selector_dump() 傾印函式（SELECTOR_LIST / COMPLEX / COMPOUND / simple 層次輸出）
  - css_parse_selector_list() stub（回傳 NULL）
  - css_selector_specificity() stub（回傳 {0,0,0}）
  - Makefile 更新（SRC 加入 src/css_selector.c）
  - 編譯零警告驗證通過
  - make test-all 全部通過

- [x] P2a Task 4: Compound selector 解析
  - CV 輔助函式（cv_is, cv_is_delim, cv_token_value）
  - parse_attribute_selector（[attr], [attr=val], [attr~=val] 等 7 種匹配）
  - parse_compound_selector（type/universal + subclass: id/class/attr/pseudo）
  - 編譯零警告驗證通過
- [x] P2a Task 5: Complex selector + selector list 解析
  - parse_complex_selector（compound + combinator 迴圈）
  - css_parse_selector_list（comma 分隔 → 多個 complex selector）
  - 錯誤處理：任一失敗 → 整個列表無效
  - 編譯零警告驗證通過
- [x] P2a Task 6: Specificity 計算
  - css_selector_specificity（a=id, b=class/attr/pseudo-class, c=type/pseudo-element）
  - 編譯零警告驗證通過
  - AddressSanitizer 驗證：零記憶體錯誤
  - make test-all 全部通過

- [x] P2b Task 7: Selector 解析整合到 AST 和 Parser
  - css_selector.h: css_selector_list 改為命名 struct（支援 forward declaration）
  - css_ast.h: 加入 `struct css_selector_list;` forward declaration + selectors 欄位
  - css_ast.c: `#include "css_selector.h"` + qualified_rule_free 釋放 selectors
  - css_parser.c: `#include "css_selector.h"` + 後處理 selector 解析 + dump 整合
  - 編譯零警告驗證通過
  - make test-all 全部通過

- [x] P2b Task 8: Selector 測試檔案 + ASAN 驗證
  - tests/selectors.css 全面測試（type/universal/class/id/compound/combinator/attribute/pseudo/complex/list）
  - Makefile 新增 test-selectors 目標，更新 test-all
  - make clean && make css_parse 零警告
  - make test-all 全部通過
  - AddressSanitizer 驗證：零記憶體錯誤（所有測試檔案）

### 未完成

- [ ] P2c: Selector 進階功能（:not(), :is(), :has(), :nth-child() 等）
