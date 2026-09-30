# lang —— 语言表

放什么：**一个** `language.json`，里面管所有语言。

```json
{
  // 允许注释
  "app.name":     { "en": "JiaoZiPi",  "zh": "饺子皮", "ja": "餃子の皮" },
  "menu.library": { "en": "Library",   "zh": "游戏库", "ja": "ライブラリ" },
  "library.count":{ "en": "{} games",  "zh": "共 {} 个游戏" }
}
```

规则：

- 外层 key 用点分扁平写法（`menu.library`），一行一条，**不按语言拆文件**
- 值是 `{ 语言代号: 文本 }`；语言代号随意取名（`en` / `zh` / `ja` / `zh_CN` ...），代码不用改就能识别
- 某个 key 缺某语言时不报错，按「当前语言 → 同语言族 → 回退语言（默认 en）→ key 本身」回退
- 允许 `//` 与 `/* */` 注释；非法 JSON 会导致 `LoadFile` 返回 false 并写入 `LastError()`

约定：

- 新增语言 = 给需要的 key 加一列；新增文案 = 先加 `en`，再补其它语言
- 层级用点分 key 表达，不要嵌套 JSON 对象
- 漏翻的 key 会被 `Translator::MissingKeys()` 记录，可在启动日志里打出来

完整用法见 [i18n 模块文档](../../src/utils/i18n/README.md)。
