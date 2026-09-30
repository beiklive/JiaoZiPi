# utils —— 通用工具与独立模块

| 路径 | 说明 |
| --- | --- |
| [log/](log/README.md) | 日志系统：等级过滤、控制台 / 文件（实时写入、按大小轮转）/ 回调输出端；底层用 spdlog |
| [i18n/](i18n/README.md) | 多语言系统：单个 `language.json`（key → { 语言: 文本 }）+ 运行时注册 + 回退链；解析用 nlohmann/json |
| [format/](format/README.md) | `{}` 字符串格式化，header-only |
| [JiaoZiPiMachine.h](JiaoZiPiMachine.h) | 机种枚举与徽标配色规范 |

约定：

- `log/`、`i18n/`、`format/` 不依赖本项目任何代码，可整目录复制到其它工程（迁移步骤见各自 README）
- 第三方库只做 header-only 依赖：`log/` 需要 `third_party/spdlog`，`i18n/` 需要 `third_party/json`（子模块初始化见 [third_party/README.md](../../third_party/README.md)）；`format/` 零第三方
- 与本项目绑定的工具（如机种定义）单独放本目录根下，不与可迁移模块混放
