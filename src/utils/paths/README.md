# paths —— 数据根目录管理

独立模块，不依赖任何宿主项目代码，可复制到其他 C++17 工程。目录规范来源：[docs/data_paths.md](../../../docs/data_paths.md)。

## 职责

- 保存/查询**数据根目录**，并拼出规范里的各类路径（配置、存档、NAND、BIOS、主题、着色器、遮罩、缩略图、核心、缓存）
- 递归创建目录、判断目录是否真的可写（探针文件）
- 解析覆盖来源：`--data-dir=<path>` / `--data-dir <path>`，其次环境变量 `JIAOZIPI_DATA_DIR`
- 路径规范化与 key 规范化（机种名/游戏 ID/核心名统一小写、非法字符换 `_`）
- **可执行文件位置**（Windows / macOS / Linux / Switch），以及由它推导的默认数据根与只读资源根

平台无关：桌面与 Switch 的默认根由本模块从可执行文件位置推导（`DefaultDataRoot()`），iOS / Android 的根由平台层提供。

## 目录布局

```text
<root>/config/{frontend.json, platforms/, cores/, games/}
<root>/data/{saves/, states/, nand/}
<root>/system/{bios/, database/}
<root>/media/{themes/, shaders/<后端>/, overlays/, thumbnails/, fonts/, icons/}
<root>/cores/
<root>/cache/
<root>/playlists/          playlists/<机种或分类>.json
```

首次启动只创建 `config/`，其余按需懒创建（`EnsureDataDirectories()` 建 1 个，`EnsureAllDirectories()` 建全部 7 个顶层目录）。

`platforms/` 指**机种**（gba / ps1 / 3ds），与 `src/platform/` 的宿主平台（Switch / Windows）不是一回事。

## 依赖

无第三方依赖；目录操作只用 `<sys/stat.h>` / `<direct.h>`（Windows）。

## 快速开始

```cpp
#include "paths/DataPaths.h"

int main(int argc, char** argv) {
    std::string root = paths::ResolveDataRootOverride(argc, argv);   // --data-dir > 环境变量
    if (root.empty()) {
        root = paths::DefaultDataRoot(argc, argv);                   // 桌面 / Switch 从可执行文件推导
    }
    if (root.empty()) {
        root = platform::MobileDataRoot();                           // 仅 iOS / Android
    }
    paths::SetDataRoot(root);
    if (!paths::EnsureDataDirectories()) {                           // 建 root + config
        // paths::DataRoot::Default().LastError() 里有原因，按规范降级
    }

    std::string config  = paths::FrontendConfigPath();                        // config/frontend.json
    std::string machine = paths::PlatformConfigPath("gba");                   // config/platforms/gba.json
    std::string save    = paths::SaveFilePath("gba", gameId);                 // data/saves/gba/<game-id>.sav
    std::string state   = paths::StateFilePath("gba", gameId, 0);             // data/states/gba/<game-id>.st0
    std::string nand    = paths::NandDirectory("3ds");                        // data/nand/3ds
    std::string bios    = paths::BiosDirectory("ps1");                        // system/bios/ps1
    std::string shader  = paths::ShaderDirectory(paths::shader::kVulkan);     // media/shaders/vulkan
    std::string core    = paths::CoreFilePath("mgba", ".nro");                // cores/mgba.nro
    std::string list    = paths::PlaylistPath("gba");                         // playlists/gba.json
}
```

## API

| 分类 | 接口 |
| --- | --- |
| 根目录 | `DataRoot::Default()`、`Set/Get/IsSet/Clear/LastError`、`SetDataRoot()`、`DataRootPath()`、`DataPath(rel)` |
| 创建目录 | `EnsureDataDirectories()`（root + config）、`EnsureAllDirectories()`（6 个顶层）、`DataRoot::Ensure(name)`、`EnsureStartupLayout()`、`EnsureLayout()` |
| 路径拼接 | `Join`、`Normalize`、`ParentDirectory`、`FileName`、`IsAbsolute`、`NormalizeKey` |
| 可执行文件 | `ExecutablePath(argc, argv)`、`ExecutableDirectory(argc, argv)`、`DefaultDataRoot(argc, argv)`、`DefaultResourceRoot(argc, argv)`、`kAppDirectoryName` |
| 常用路径 | `FrontendConfigPath`、`PlatformConfigPath`、`CoreConfigPath`、`GameConfigPath`、`SaveFilePath`、`StateFilePath`、`NandDirectory`、`BiosDirectory`、`DatabaseDirectory`、`ThemeDirectory`、`ShaderDirectory`、`OverlayDirectory`、`ThumbnailDirectory`、`CoreFilePath`、`CacheDirectory`、`PlaylistDirectory`、`PlaylistPath` |
| 文件系统 | `MakeDirectories`、`DirectoryExists`、`FileExists`、`IsWritableDirectory` |
| 覆盖解析 | `ResolveDataRootOverride(argc, argv, env)`、`DataRootFromEnvironment()` |
| 常量 | `kStartupDirectories`、`kRootDirectories`、`sub::kConfig/kData/kSystem/kMedia/kCores/kCache`、`sub::kPlatforms/kGames/kSaves/kStates/kNand/kBios/kDatabase/kThemes/kShaders/kOverlays/kThumbnails/kFonts/kIcons/kPlaylists`、`shader::kVulkan/kOpenGl`、`kStartupDirectories`、`kRootDirectories` |

- `NormalizeKey()`：`"GBA"` → `gba`，`"Pokemon FireRed/Leaf"` → `pokemon_firered_leaf`，空结果 → `unknown`；
  机种目录名请直接用 `jzp::MachineKey(machine)`（3DS 是 `3ds`）
- `ExecutablePath()`：Windows 用 `GetModuleFileNameW()`，macOS 用 `_NSGetExecutablePath()` + `realpath()`，
  Linux 用 `readlink("/proc/self/exe")`，Switch 用 `argv[0]`（**必须传入 `argc/argv`**）
- `DefaultDataRoot()`：桌面 → `<可执行文件目录>/JiaoZiPi`，Switch → `sdmc:/JiaoZiPi`，iOS/Android → 空串（平台层给）
- 未设置根目录时，各路径函数返回相对路径（`config/platforms/gba.json`），不抛错；`Ensure*()` 返回 `false` 并写入 `LastError()`
- `DataRoot` 线程安全（内部加锁）

## 迁移到其他项目

复制 `paths/` 一个目录即可（不依赖同仓库其它模块）：

```bash
c++ -std=c++17 -I src/utils src/utils/paths/DataPaths.cpp your_app.cpp -o your_app
```

## 注意

- `MakeDirectories()` 已存在视为成功；`sdmc:` / `C:` 这类设备前缀不会被当成待创建目录
- `IsWritableDirectory()` 会写探针文件再删除，用于启动时判断包体是否只读（`Program Files`、`.app`、Flatpak）并决定降级
- 覆盖值会被 `Normalize()`：相对路径保持相对（相对于进程当前工作目录），需要绝对路径请由调用方解析
- `cache/` 在 iOS / Android 建议由平台层单独指到 `Library/Caches` / `getCacheDir()`，其余目录仍在数据根下
