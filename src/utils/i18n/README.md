# i18n —— 通用多语言系统

独立模块，不依赖任何宿主项目代码，可直接复制到其他 C++17 工程。

## 特性

- 语言表来自 UTF-8 文本文件，也可运行时注册（把翻译编进二进制，适合无文件系统的设备）
- 查找链：**当前语言 → 同语言族（zh_TW → zh_CN）→ 回退语言 → key 本身**
- `{}` 占位符格式化：`Tr("library.count", 12)`
- 缺失 key 记录，便于查漏
- 线程安全

## 依赖

仅同仓库的 `format/StrFormat.h`（header-only）。`i18n/` 与 `log/` 互不依赖。

## 语言文件格式

UTF-8 文本，文件名即语言名（`zh_CN.lang` / `en_US.lang`）：

```text
# 注释
app.name = 饺子皮
library.scanning = 正在扫描：{}
library.count = 共 {} 个游戏
```

- `#` 或 `;` 开头为注释；key/value 两侧空白会被忽略
- 值里可用 `\n` `\t` `\\` 转义
- 值里若需要字面量 `{`，写 `{{`（仅在带参数调用时才会被解析）

## 快速开始

```cpp
#include "i18n/I18n.h"

i18n::Translator& tr = i18n::Translator::Default();
tr.SetFallbackLocale("en_US");
tr.LoadDirectory("resources/lang");          // 扫描目录下所有 .lang
tr.SetLocale("zh_CN");                       // 返回 false 表示该语言没有翻译（仍会切换并回退）

std::string title = tr.Tr("app.name");       // 饺子皮
std::string msg   = tr.Tr("library.count", 12);  // 共 12 个游戏

// 全局便捷接口（走 Default()）
i18n::SetLocale("en_US");
i18n::Tr("menu.settings");
```

编译进二进制的内置语言表：

```cpp
tr.Register("zh_CN", {
    {"app.name", "饺子皮"},
    {"menu.quit", "退出"},
});
```

排查漏翻译：

```cpp
for (const auto& key : tr.MissingKeys()) {
    LOG_WARN("缺少翻译：{}", key);
}
```

## 迁移到其他项目

1. 复制 `format/` 与 `i18n/` 两个目录；
2. 头文件搜索路径指向它们的上级目录（例如 `-I<root>/utils`）；
3. 把 `I18n.cpp` 加入构建。

```bash
c++ -std=c++17 -I src/utils src/utils/i18n/I18n.cpp your_app.cpp -o your_app
```

## 注意

- `LoadDirectory` 用 POSIX `dirent` / Windows `FindFirstFileA`，不依赖 `std::filesystem`，便于在 devkitPro 等精简环境使用；资源路径由调用方决定（Switch 上通常是 romfs 路径）。
- 字符串统一按 UTF-8 处理，渲染字体需自行支持对应字形。
