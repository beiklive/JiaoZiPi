# 饺子皮 (JiaoZiPi)

多核心模拟器前端

## 简介

饺子皮是一个多核心的模拟器前端，把多个平台的模拟器核心聚合到同一套界面与运行框架下，提供统一的操作方式、统一的配置入口和统一的增强功能。

目标是：**一套前端，管理所有核心**。用户不需要为每个平台分别安装、配置、记忆不同的操作方式，只需要在饺子皮中加载 ROM、选择核心即可运行。

## 支持的核心

| 平台 | 说明 |
| --- | --- |
| FC | 红白机 / NES |
| GB | Game Boy |
| GBC | Game Boy Color |
| GBA | Game Boy Advance |
| NDS | Nintendo DS |
| 3DS | Nintendo 3DS |
| SFC | 超级任天堂 / SNES |
| MD | Mega Drive / Genesis |
| SS | Sega Saturn |
| DC | Dreamcast |
| NGC | Nintendo GameCube |
| WII | Nintendo Wii |
| PS1 | PlayStation |
| PSP | PlayStation Portable |
| ARCADE | 街机 |

> 核心列表会持续扩充，后续将加入更多平台。

## 功能特性

- **多核心** — 统一前端调度不同平台的模拟器核心，核心与界面解耦。
- **高度自定义** — 按键映射、画面布局、核心参数、界面行为均可按用户习惯调整。
- **金手指** — 内置金手指支持，用于修改游戏内数值、解锁内容等。
- **快进** — 可加速运行，跳过重复的练级、刷取与过场流程。
- **倒带** — 支持回溯到之前的时间点，取代传统即时存档的反复读取。
- **遮罩** — 支持叠加遮罩层（背景边框 / 屏幕效果），提升画面观感。
- **滤镜** — 支持画面滤镜（放大、扫描线、色彩校正等)，改善原始像素画面的显示效果。

## 设计方向

- 核心与前端分离：新增平台只需接入新核心，不改动前端整体结构。
- 配置集中管理：核心、按键、画面、增强功能通过统一配置生效。
- 增强功能通用化：金手指、快进、倒带、遮罩、滤镜作为前端能力提供，尽量不依赖单个核心实现。

## 目录结构

```text
JiaoZiPi/
├── src/                    # 代码目录
│   ├── app/                # 程序入口与生命周期
│   ├── core/               # 核心加载与管理（核心接入层）
│   ├── emulator/           # 运行调度：暂停 / 快进 / 倒带 / 存档
│   ├── config/             # 配置系统
│   ├── input/              # 输入与按键映射
│   ├── ui/                 # 界面
│   ├── platform/           # 平台适配层
│   └── utils/              # 通用工具
│       ├── log/            # 日志系统（独立模块，可整体迁移）
│       ├── i18n/           # 多语言系统（独立模块，可整体迁移）
│       ├── format/         # 字符串格式化（header-only，供上面两个模块共用）
│       ├── paths/          # 数据根目录管理（设置路径 / 建目录 / 覆盖解析）
│       └── JiaoZiPiMachine.h  # 机种枚举与徽标配色
├── resources/              # 资源目录（只读，随包发布）
│   ├── platforms/          # 各机种内置配置（机种名 / 默认核心 / 后缀）
│   ├── fonts/              # 字体
│   ├── icons/              # 图标
│   ├── images/             # 图片
│   ├── sounds/             # 音效
│   ├── lang/               # 语言表（单个 language.json）
│   ├── shaders/            # 滤镜
│   ├── overlays/           # 遮罩
│   ├── cheats/             # 金手指
│   └── themes/             # 主题
├── third_party/            # 第三方库（子模块）与模拟器核心
│   ├── spdlog/             # 日志底层（v1.17.0，header-only 使用）
│   ├── json/               # JSON 解析 nlohmann/json（v3.12.0，header-only 使用）
│   ├── miniz/              # zip 解压（3.1.2，纯 C）
│   └── libarchive/         # 7z 解压（v3.8.9，BSD-2）
├── docs/                   # 文档
├── tools/                  # 辅助脚本与工具
├── tests/                  # 测试
├── .gitignore
└── README.md
```

约定：

- 代码统一放在 `src/`，头文件与实现文件同目录。
- 新增平台适配放在 `src/platform/`，不改动上层逻辑。
- 所有运行时资源放在 `resources/`，按类型分目录。
- 模拟器核心源码或子模块放在 `third_party/`，与前端代码隔离。

## 内置基础模块

`src/utils/` 下的 `log/` 与 `i18n/` 是不依赖本项目任何代码的独立模块，可整目录复制到其他工程：

- 日志：等级过滤、控制台 / 文件（实时写入、按大小轮转）/ 回调输出端，底层用 spdlog，用法见 [log/README.md](src/utils/log/README.md)
- 多语言：单个 [language.json](resources/lang/language.json) 管所有语言（`key → { 语言: 文本 }`），查找链「当前语言 → 同语言族 → 回退语言 → key」，用法见 [i18n/README.md](src/utils/i18n/README.md)
- 数据目录：设置/拼接数据根路径、递归建目录、可写探测，支持 `--data-dir` 与环境变量覆盖，规范见 [docs/data_paths.md](docs/data_paths.md)，用法见 [paths/README.md](src/utils/paths/README.md)

日志与多语言对第三方库只做 header-only 依赖（spdlog / nlohmann-json）；zip / 7z 解压用 miniz + libarchive。
首次克隆后先拉取子模块：

```bash
git submodule update --init --recursive
```

冒烟测试：

```bash
sh tools/build_utils_test.sh
```

## 开发计划

- [x] 项目立项，明确多核心前端定位
- [ ] 接入首批核心
- [ ] 完成统一配置系统（按键 / 画面 / 核心参数）
- [ ] 实现金手指、快进、倒带
- [ ] 实现遮罩与滤镜
- [ ] 持续扩充支持的核心

## 反馈与贡献

欢迎通过 Issue 反馈问题、提交核心适配建议或功能需求。

## 许可证

本项目以 **MIT** 许可证发布，见 [LICENSE](LICENSE)。

第三方库（子模块）遵循各自上游许可证：spdlog / nlohmann-json / miniz 为 MIT，libarchive 为 BSD-2-Clause，明细见 [third_party/README.md](third_party/README.md)。

模拟器核心是独立项目，各自遵循上游许可证；随包分发时必须一并保留其许可证文件。

> 注意：核心以独立模块（`.nro` / `.so` / `.dll`）方式加载时，本项目可维持 MIT；若改为静态链接 GPL 系核心（Dolphin、PPSSPP、YabaSanshiro 等），本项目需相应改为 GPL 兼容许可证。
