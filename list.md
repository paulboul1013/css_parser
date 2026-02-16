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

- [ ] Task 4: 空白/標點/Comment token
- [ ] Task 5: 數值 token
- [ ] Task 6: Ident/Function/Hash/At token
- [ ] Task 7: String 和 URL token
- [ ] Task 8: Tokenizer 完整性驗證
- [ ] Task 9: AST 結構定義
- [ ] Task 10: Parser 骨架
- [ ] Task 11: At-rule 和 Qualified rule
- [ ] Task 12: Declaration 與 !important
- [ ] Task 13: 端到端整合測試
- [ ] Task 14: 記憶體驗證與文件更新
