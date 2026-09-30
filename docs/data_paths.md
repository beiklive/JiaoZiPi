# 数据目录规范

JiaoZiPi 的可写数据（配置、存档、日志、缓存、用户资源）统一放在应用根目录下，各平台只有一个根路径：

| 平台 | 数据根目录 | 解析方式 |
| --- | --- | --- |
| Windows | `<可执行文件目录>/JiaoZiPi` | `GetModuleFileNameW()` → 取目录 → 拼 `JiaoZiPi` |
| macOS | `<可执行文件目录>/JiaoZiPi` | `_NSGetExecutablePath()` → 取目录 → 拼 `JiaoZiPi`（`.app` 内即为 `Contents/MacOS/JiaoZiPi`） |
| Linux | `<可执行文件目录>/JiaoZiPi` | `readlink("/proc/self/exe")` → 取目录 → 拼 `JiaoZiPi` |
| iOS | `<App 沙盒>/Documents/JiaoZiPi` | `FileManager.urls(for: .documentDirectory, in: .userDomainMask)` |
| Android | `/sdcard/JiaoZiPi` | 固定路径（`/sdcard` = `/storage/emulated/0`，需外部存储权限，见风险） |
| Switch | `sdmc:/JiaoZiPi` | 固定前缀 |

大小写统一写 `JiaoZiPi`：Linux 区分大小写，写错会落到另一个目录；Windows / macOS / Switch(FAT/exFAT) 不区分，但保持一致避免打包/脚本不一致。

## 根目录下的布局

```text
<数据根>/JiaoZiPi/
├── config/     配置（general.json 等）
├── saves/      游戏存档（电池存档）
├── states/     即时存档与倒带快照
├── logs/       日志（实时写入）
├── cache/      可重建缓存（封面、数据库索引等）
├── lang/       用户语言表（覆盖内置 language.json）
├── cheats/     用户金手指
├── shaders/    用户滤镜
├── overlays/   用户遮罩
└── themes/     用户主题
```

## 只读资源 vs 可写数据

| 类型 | 位置 | 说明 |
| --- | --- | --- |
| 内置资源（字体、图标、内置 `language.json`、内置滤镜/遮罩） | 打包内的 `resources/`：Switch 为 `romfs:/`，Android 为 `assets/`，iOS/macOS 为 app bundle，Windows/Linux 为可执行文件旁的 `resources/` | **只读**，永不写入 |
| 用户数据与覆盖（上表中的目录） | 数据根目录 | 读写 |

加载顺序统一为：**数据根目录 → 内置资源**（前者存在就用前者，例如 `lang/language.json` 覆盖内置语言表），这样用户不改包体也能替换本地化与素材。

## 路径约定

- 统一使用正斜杠 `/`，路径末尾不带分隔符；Win32 的 `fopen`/`CreateFile` 接受 `/`
- 路径按 UTF-8 处理，Windows 侧转换用 `MultiByteToWideChar(CP_UTF8, ...)`，不要用 ANSI API
- 上层不拼字符串：从 `src/platform/` 的路径服务取，只拼子目录名（如 `Paths().data + "/saves"`）

建议接口：

```cpp
struct AppPaths {
    std::string root;      // <数据根>/JiaoZiPi
    std::string config;    // root/config
    std::string saves;     // root/saves
    std::string states;    // root/states
    std::string logs;      // root/logs
    std::string cache;     // root/cache
    std::string lang;      // root/lang
    std::string cheats;    // root/cheats
    std::string shaders;   // root/shaders
    std::string overlays;  // root/overlays
    std::string themes;    // root/themes
    std::string resources; // 只读资源根（romfs:/ / assets / bundle / 可执行文件旁 resources）
};

const AppPaths& Paths();
bool EnsureAppDirectories();   // 启动时一次性创建，失败写日志并降级
```

## 覆盖机制

- 环境变量 `JIAOZIPI_DATA_DIR` 优先于平台默认值
- 命令行 `--data-dir=<path>` 优先于环境变量
- 便携部署与自动化测试都走这条，避免依赖可执行文件位置

## 风险与失败处理

| 平台 | 风险 | 处理 |
| --- | --- | --- |
| Windows | 装在 `Program Files` 下无写权限 | 创建/写入失败 → 记录日志 → 回退到 `%LOCALAPPDATA%\JiaoZiPi` |
| macOS | 从 `.app` 运行且包体只读（签名、只读卷、`/Applications` 权限）时不可写 | 同上；开发期直接从命令行运行二进制不受影响 |
| Linux | 装在 `/usr`、Flatpak/Snap 沙盒下不可写 | 同上，回退到 `$XDG_DATA_HOME/JiaoZiPi` |
| iOS | `Documents` 内容默认随 iCloud 备份；用户在「文件」App 中可见（开启分享时） | 备份体积敏感的数据（缓存）改放 `Library/Caches`；不要把路径写死为 home 目录 |
| Android | Android 10+ 分区存储：`/sdcard/JiaoZiPi` 需要「所有文件访问」权限（`MANAGE_EXTERNAL_STORAGE`），Google Play 上架会受限；Android 11+ 对 `/sdcard/Android/data` 访问也收紧 | 优先尝试 `/sdcard/JiaoZiPi`；无权限时回退 `getExternalFilesDir()`（`/sdcard/Android/data/<pkg>/files/JiaoZiPi`），并在界面提示 |
| Switch | SD 卡未插入或写失败（`sdmc:` 返回错误） | 创建失败 → 不写入文件日志（只留控制台/回调输出）并提示用户 |

所有平台创建目录都按需 `mkdir`，已存在不算失败；只处理"目录创建失败"这一种错误，不做逐级权限探测。
