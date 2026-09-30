# lang —— 语言表

放什么：翻译文件，文件名即语言代号（`zh_CN` / `en_US` / `ja_JP`）。

两种写法都支持，可混用，后缀 `.lang` 或 `.json` 均可：

```text
# 文本写法（.lang）
app.name = 饺子皮
library.count = 共 {} 个游戏
```

```json
// JSON 风格写法（.json）：引号、末尾逗号、花括号都可省略
{
  "app.name": "餃子の皮",
  "menu.library": "ライブラリ",
  "greeting": "こんにちは、{}"
}
```

规则（由 `src/utils/i18n/I18n.cpp` 的自制行式解析器处理）：

- 一行一条，key 用点分扁平写法（`menu.library`），**不支持嵌套对象**
- 引号可选；`"key": "value",` 与 `key = value` 等价；值里的逗号、`#` 在引号内不会被截断
- 注释：`#`、`;`、`//` 开头的行
- 转义：`\n \t \r \b \f \" \/ \\` 与 `\uXXXX`（BMP）
- 解析不了的行会被跳过，对应的 key 会出现在 `Translator::MissingKeys()` 里

约定：

- key 与 `en_US.lang` 对齐，新增 key 先加英文再补其它语言
- 层级用点分 key 表达，不要用 JSON 嵌套（解析器不处理嵌套）

完整说明见 [i18n 模块文档](../../src/utils/i18n/README.md)。
