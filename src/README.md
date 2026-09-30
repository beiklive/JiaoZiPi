# src —— 代码目录

按职责分层，头文件与实现文件同目录。

| 目录 | 职责 |
| --- | --- |
| [app/](app/README.md) | 程序入口与生命周期 |
| [core/](core/README.md) | 模拟器核心加载与管理 |
| [emulator/](emulator/README.md) | 运行调度（暂停 / 快进 / 倒带 / 存档） |
| [config/](config/README.md) | 配置系统 |
| [input/](input/README.md) | 输入与按键映射 |
| [ui/](ui/README.md) | 界面 |
| [platform/](platform/README.md) | 平台适配层 |
| [utils/](utils/README.md) | 通用工具与可独立迁移的模块 |

约定：上层只通过 `core/` 暴露的接口访问模拟器核心；平台差异收敛在 `platform/`，其余目录不出现平台分支。
