# paths —— 数据根目录管理

独立模块，不依赖任何宿主项目代码，可复制到其他 C++17 工程。规范来源：[docs/data_paths.md](../../../docs/data_paths.md)。

## 职责

- 保存/查询**数据根目录**，并拼出子目录与文件路径
- 递归创建目录、判断目录是否真的可写（探针文件）
- 解析覆盖来源：`--data-dir=<path>` / `--data-dir <path>`，其次环境变量 `JIAOZIPI_DATA_DIR`
- 路径规范化：`\` → `/`、折叠重复斜杠、去末尾斜杠，保留 `sdmc:` / `C:` / `//UNC` 前缀

平台无关：**默认根路径由平台层算好后传进来**（Windows/macOS/Linux 取可执行文件目录，iOS 取 Documents，Android 用 `/sdcard`，Switch 用 `sdmc:`）。

## 依赖

无第三方依赖；只用 `<sys/stat.h>` / `<direct.h>`（Windows）做目录操作。

## 快速开始

```cpp
#include "paths/DataPaths.h"

int main(int argc, char** argv) {
    // 1) 覆盖优先：--data-dir / 环境变量
    std::string root = paths::ResolveDataRootOverride(argc, argv);
    // 2) 否则用平台层算出的默认根（示意）
    if (root.empty()) {
        root = platform::DefaultDataRoot();   // "sdmc:/JiaoZiPi" 等
    }
    paths::SetDataRoot(root);

    // 3) 按规范创建根目录与 10 个子目录，失败原因见 DataRoot::LastError()
    if (!paths::EnsureDataDirectories()) {
        // 记录降级信息，例如回退到用户目录
    }

    const std::string config = paths::DataPath("config/general.json");
    const std::string logs = paths::DataPath(paths::sub::kLogs);
}
```

## API

| 分类 | 接口 |
| --- | --- |
| 根目录 | `DataRoot::Default()`、`Set()` / `Get()` / `IsSet()` / `Clear()`、`DataRootPath()`、`SetDataRoot()` |
| 拼接 | `DataPath(rel)`、`DataRoot::Sub(name)`、`Join(base, child)`、`Normalize(path)`、`ParentDirectory()`、`FileName()`、`IsAbsolute()` |
| 目录操作 | `MakeDirectories()`、`DirectoryExists()`、`FileExists()`、`IsWritableDirectory()` |
| 创建规范目录 | `EnsureDataDirectories()`、`DataRoot::Ensure(name)`、`DataRoot::EnsureLayout()` |
| 覆盖解析 | `ResolveDataRootOverride(argc, argv, env = "JIAOZIPI_DATA_DIR")`、`DataRootFromEnvironment()` |
| 子目录常量 | `paths::sub::kConfig / kSaves / kStates / kLogs / kCache / kLang / kCheats / kShaders / kOverlays / kThemes`、`paths::kSubDirectories` |

`DataRoot` 线程安全（内部加锁）；`IsSet()==false` 时 `Ensure*()` 返回 false 并写入 `LastError()`。

## 迁移到其他项目

复制 `paths/` 一个目录即可（它不依赖同仓库其它模块）：

```bash
c++ -std=c++17 -I src/utils src/utils/paths/DataPaths.cpp your_app.cpp -o your_app
```

## 注意

- `MakeDirectories()` 已存在视为成功；`sdmc:` / `C:` 这类设备前缀不会被当成待创建目录。
- `IsWritableDirectory()` 会写一个探针文件再删除；只读环境（`Program Files`、`.app` 包体、Flatpak）用它在启动时决定要不要降级。
- 覆盖值会被 `Normalize()`：相对路径保持相对（相对于进程当前工作目录），需要绝对路径请由调用方解析。
