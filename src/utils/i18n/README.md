# i18n —— 通用多语言系统

独立模块，不依赖任何宿主项目代码，可直接复制到其他 C++17 工程。

## 特性

- 语言表来自 UTF-8 文件，两种格式自动识别（按首个非空字符判断）：
  - 文本：`key = value`
  - JSON：嵌套对象按 `.` 展平
- 也可运行时注册（把翻译编进二进制，适合无文件系统的设备）
- 查找链：**当前语言 → 同语言族（zh_TW → zh_CN）→ 回退语言 → key 本身**
- `{}` 占位符格式化：`Tr("library.count", 12)`
- 缺失 key 记录，便于查漏
- 线程安全

## 依赖

| 依赖 | 说明 |
| --- | --- |
| `format/StrFormat.h` | 同仓库，header-only |
| [nlohmann/json](https://github.com/nlohmann/json) v3.12.0 | `third_party/json` 子模块，**可选**：用 `__has_include` 探测，没有该头文件时自动降级为仅支持文本表，其余功能不受影响 |

运行时可用 `i18n::Translator::JsonAvailable()` 查询本次编译是否带上了 JSON 支持。

## 语言文件格式

文件编码 UTF-8，文件名即语言名（`zh_CN.lang` / `en_US.lang`）。

文本表：

```text
# 注释
app.name = 饺子皮
library.scanning = 正在扫描：{}
library.count = 共 {} 个游戏
```

JSON 表（`ja_JP.json`，嵌套会被展平成点分 key）：

```json
{
  "app": { "name": "餃子の皮" },
  "menu": { "library": "ライブラリ" },
  "greeting": "こんにちは、{}"
}
```

- `#` 或 `;` 开头为注释（文本表）；JSON 里允许 `//` 与 `/* */` 注释
- 值里可用 `\n` `\t` `\\` 转义（文本表）
- 值里若需要字面量 `{`，写 `{{`（仅在带参数调用时才会被解析）
- JSON 只接受字符串值（数字、布尔会转成文本），数组/对象以外的复杂结构会被忽略

## 快速开始

```cpp
#include "i18n/I18n.h"

i18n::Translator& tr = i18n::Translator::Default();
tr.SetFallbackLocale("en_US");
tr.LoadDirectory("resources/lang", ".lang");   // 文本表
tr.LoadDirectory("resources/lang", ".json");   // JSON 表（需 nlohmann/json）
tr.SetLocale("zh_CN");                         // 返回 false 表示该语言没有翻译（仍会切换并回退）

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
2. 头文件搜索路径指向它们的上级目录（例如 `-I<root>/utils`）；需要 JSON 表时再加入 nlohmann/json 的 include 目录；
3. 把 `I18n.cpp` 加入构建。

```bash
# 仅文本表
c++ -std=c++17 -I src/utils src/utils/i18n/I18n.cpp your_app.cpp -o your_app

# 带 JSON 表
c++ -std=c++17 -I src/utils -I third_party/json/include \
    src/utils/i18n/I18n.cpp your_app.cpp -o your_app
```

## 注意

- `LoadDirectory` 用 POSIX `dirent` / Windows `FindFirstFileA`，不依赖 `std::filesystem`，便于在 devkitPro 等精简环境使用；资源路径由调用方决定（Switch 上通常是 romfs 路径）。
- 字符串统一按 UTF-8 处理，渲染字体需自行支持对应字形。
- nlohmann/json 是重量级模板库，会明显增加 `I18n.cpp` 的编译时间；只影响该 .cpp，不影响包含 `I18n.h` 的业务文件。
