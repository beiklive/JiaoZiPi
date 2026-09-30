# lang —— 语言表

放什么：翻译文件，文件名即语言代号。

| 格式 | 示例 | 说明 |
| --- | --- | --- |
| 文本 | `zh_CN.lang`、`en_US.lang` | `key = value`，`#` / `;` 注释 |
| JSON | `ja_JP.json` | 嵌套对象按 `.` 展平，需 nlohmann/json |

```text
app.name = 饺子皮
library.count = 共 {} 个游戏
```

```json
{ "menu": { "library": "游戏库" }, "greeting": "你好，{}" }
```

约定：

- key 与 `en_US.lang` 对齐，新增 key 先加英文再补其它语言
- 缺失 key 会被 `Translator::MissingKeys()` 记录，方便查漏
- 完整格式说明见 [i18n 模块文档](../../src/utils/i18n/README.md)
