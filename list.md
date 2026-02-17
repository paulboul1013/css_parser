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
- [ ] Task 10: Parser 骨架
- [ ] Task 11: At-rule 和 Qualified rule
- [ ] Task 12: Declaration 與 !important
- [ ] Task 13: 端到端整合測試
- [ ] Task 14: 記憶體驗證與文件更新
