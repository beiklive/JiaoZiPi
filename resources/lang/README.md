# lang —— 语言表

放什么：`<locale>.lang` 翻译文件，如 `zh_CN.lang`、`en_US.lang`。

格式（UTF-8，`key = value`）：

```text
app.name = 饺子皮
library.count = 共 {} 个游戏
```

约定：

- 新增语言 = 新增一个文件，文件名即语言代号
- key 与 `en_US.lang` 对齐，新增 key 先加英文再补其它语言
- 完整说明见 [i18n 模块文档](../../src/utils/i18n/README.md)
