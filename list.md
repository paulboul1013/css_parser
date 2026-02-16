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

## 未完成

- [ ] Task 2: Token 結構與生命週期
- [ ] Task 3: Tokenizer 前處理與 UTF-8
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
