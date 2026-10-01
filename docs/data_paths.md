# 数据目录规范

JiaoZiPi 的可写数据（配置、存档、日志、缓存、用户素材）统一放在应用根目录下，各平台只有一个根路径。

## 平台根目录

| 平台 | 数据根目录 | 解析方式 |
| --- | --- | --- |
| Windows | `<可执行文件目录>/JiaoZiPi` | `paths::DefaultDataRoot()`（`GetModuleFileNameW()` → 取目录 → 拼 `JiaoZiPi`） |
| macOS | `<可执行文件目录>/JiaoZiPi` | `paths::DefaultDataRoot()`（`_NSGetExecutablePath()` + `realpath()`；`.app` 内即 `Contents/MacOS/JiaoZiPi`） |
| Linux | `<可执行文件目录>/JiaoZiPi` | `paths::DefaultDataRoot()`（`readlink("/proc/self/exe")`） |
| iOS | `<App 沙盒>/Documents/JiaoZiPi` | `FileManager.urls(for: .documentDirectory, in: .userDomainMask)` |
| Android | `/sdcard/JiaoZiPi` | 固定路径（`/sdcard` = `/storage/emulated/0`，需外部存储权限，见风险） |
| Switch | `sdmc:/JiaoZiPi` | `paths::DefaultDataRoot()` 固定前缀；可执行文件路径需传 `argv`（无 `/proc/self/exe`） |

目录名统一写 `JiaoZiPi`：Linux 区分大小写；Windows / macOS / Switch(FAT/exFAT) 不区分，但保持一致，避免打包与脚本出现两个目录。

**命名区分**：数据目录里的 `platforms/` 指**机种**（`gba` / `ps1` / `3ds` ...，见 [JiaoZiPiMachine.h](../src/utils/JiaoZiPiMachine.h)）；`src/platform/` 指**宿主平台**（Switch / Windows / Android ...）。

## 目录布局

顶层只有 6 个目录，其余全部下沉：

```text
<数据根>/JiaoZiPi/
├── config/
│   ├── frontend.json                    前端设置
│   ├── platforms/<机种>.json             每个机种的全局配置
│   ├── cores/<核心>.json                 每个核心的设置
│   └── games/<机种>/<game-id>.json       每个游戏的独立配置
├── data/
│   ├── saves/<机种>/<game-id>.sav        电池存档
│   ├── states/<机种>/<game-id>.st<槽位>   即时存档（含倒带快照）
│   └── nand/<机种>/                      虚拟 NAND / 虚拟存储（3DS、PSP、Wii、DC…）
├── system/
│   ├── bios/<机种>/                      BIOS / 固件
│   └── database/                         游戏数据库
├── media/
│   ├── themes/<主题>/                    前端主题
│   ├── shaders/vulkan/                   着色器（按后端分开）
│   ├── shaders/opengl/
│   ├── overlays/                         遮罩
│   ├── thumbnails/<机种>/                缩略图
│   ├── fonts/
│   └── icons/
├── cores/                                核心文件（mgba.nro、ppsspp.nro…）
└── cache/                                可重建（shader cache、索引、封面缓存）
```

**按需创建**：首次启动只建 `config/`，其余目录在真正写入时才创建，根目录始终保持干净。

| 内容 | 位置 |
| --- | --- |
| 前端设置 | `config/frontend.json` |
| 每个机种的全局配置 | `config/platforms/<机种>.json` |
| 每个核心的设置 | `config/cores/<核心>.json` |
| 每个游戏的独立配置 | `config/games/<机种>/<game-id>.json` |
| 3DS / PSP 虚拟 NAND 与核心自建数据 | `data/nand/3ds/`、`data/nand/psp/`、… |
| 其他机种存档 | `data/saves/<机种>/<game-id>.sav` |
| 即时存档 | `data/states/<机种>/<game-id>.st<槽位>` |
| BIOS / 固件 | `system/bios/<机种>/` |
| 游戏数据库 | `system/database/` |
| 前端主题 | `media/themes/<主题>/` |
| 着色器（Vulkan / OpenGL） | `media/shaders/vulkan/`、`media/shaders/opengl/` |
| 遮罩 | `media/overlays/` |
| 缩略图 | `media/thumbnails/<机种>/` |
| 核心文件 | `cores/<核心>.<扩展名>` |
| 缓存 | `cache/<用途>/` |
| 日志 | `logs/`（写入时才创建；见下） |

## 命名约定

- **`<机种>`** 用 [`MachineKey()`](../src/utils/JiaoZiPiMachine.h)：`fc` `sfc` `gb` `gbc` `gba` `nds` **`3ds`** `ngc` `wii` `md` `ss` `dc` `ps1` `psp` `arcade`（3DS 是 `3ds`，不是 `n3ds`）
- **`<game-id>`** 优先用核心上报的稳定 ID（卡带序列号 / 标题 ID / ROM header 校验和）；取不到时退化为「文件名去扩展名 + CRC32 短哈希」。**不要**直接用 ROM 文件名——用户改名会导致存档与配置全部失联
- 所有拼接片段先经 `paths::NormalizeKey()` 规范化：ASCII 转小写、`[a-z0-9_.-]` 之外的字符换成 `_`、压缩连续 `_`、去掉首部 `.`（防隐藏文件与 `..` 上跳），结果为空时回退 `unknown`
- 全路径统一正斜杠 `/`，末尾不带分隔符；总深度控制在 4 层内（Windows `MAX_PATH` 260）

## 只读资源 vs 可写数据

| 类型 | 位置 | 说明 |
| --- | --- | --- |
| 内置资源（字体、图标、内置 `language.json`、内置滤镜/遮罩/主题、随包数据库） | 打包内 `resources/`：Switch `romfs:/`、Android `assets/`、iOS/macOS app bundle、Windows/Linux 可执行文件旁 | **只读**，永不写入 |
| 用户数据与覆盖（上表 `config/` `data/` `system/` `media/` `cores/` `cache/`） | 数据根目录 | 读写 |

加载顺序统一为：**数据根目录 → 内置资源**（前者存在就用前者）。例如 `media/themes/` 有同名主题就覆盖内置主题，`system/database/` 有更新版数据库就用它。

## 平台差异与风险

| 平台 | 风险 | 处理 |
| --- | --- | --- |
| Windows | 装在 `Program Files` 下无写权限 | 创建/写入失败 → 记日志 → 回退 `%LOCALAPPDATA%\JiaoZiPi` |
| macOS | 从 `.app` 运行且包体只读（签名、只读卷、`/Applications` 权限）时不可写 | 同上；开发期命令行直接跑二进制不受影响 |
| Linux | 装在 `/usr`、Flatpak/Snap 沙盒下不可写 | 同上，回退 `$XDG_DATA_HOME/JiaoZiPi` |
| iOS | `Documents` 随 iCloud 备份；缩略图/缓存体积大 | **仅 `cache/`（必要时含 `media/thumbnails/`）改放 `Library/Caches/JiaoZiPi`**：不备份、可被系统清理 |
| Android | Android 10+ 分区存储：`/sdcard/JiaoZiPi` 需「所有文件访问」（`MANAGE_EXTERNAL_STORAGE`），上架审核受限 | 先试 `/sdcard/JiaoZiPi`；无权限时回退 `getExternalFilesDir()`（`/sdcard/Android/data/<pkg>/files/JiaoZiPi`）；缓存走 `getCacheDir()` |
| Switch | SD 卡未插入或写失败 | 创建失败 → 不写文件日志，只保留控制台/回调输出并提示用户 |

## 覆盖机制

优先级：`--data-dir=<path>` / `--data-dir <path>` > 环境变量 `JIAOZIPI_DATA_DIR` > 平台默认根。

便携部署与自动化测试都走这条，避免依赖可执行文件位置。

## 实现

目录的保存、拼接、创建与覆盖解析在 [src/utils/paths](../src/utils/paths/README.md)：

```cpp
// 1) 覆盖优先；2) 桌面与 Switch 用可执行文件位置推导；3) iOS/Android 由平台层提供
std::string root = paths::ResolveDataRootOverride(argc, argv);
if (root.empty()) root = paths::DefaultDataRoot(argc, argv);
if (root.empty()) root = platform::MobileDataRoot();      // 仅 iOS / Android 需要

paths::SetDataRoot(root);
paths::EnsureDataDirectories();                            // 建 root + config

std::string save   = paths::SaveFilePath(jzp::MachineKey(jzp::Machine::GBA), gameId);
std::string shader = paths::ShaderDirectory(paths::shader::kVulkan);  // media/shaders/vulkan
```

`paths::ExecutablePath()/ExecutableDirectory()` 覆盖 Windows / macOS / Linux / Switch 四个平台（Switch 需要把 `argv` 传进来）；`src/platform/` 只负责 iOS / Android 的数据根、只读资源根，以及只读环境下的降级。
