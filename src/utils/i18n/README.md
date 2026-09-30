# i18n —— 通用多语言系统

独立模块，不依赖任何宿主项目代码，可复制到其他 C++17 工程。

## 特性

- **一个 `language.json` 管所有语言**，不按语言拆文件：
  ```json
  {
    "app.name":  { "en": "JiaoZiPi", "zh": "饺子皮" },
    "menu.quit": { "en": "Quit",     "zh": "退出" }
  }
  ```
- 查找链：**当前语言 → 同语言族（zh_TW → zh）→ 回退语言 → key 本身**
- `{}` 占位符格式化：`Tr("library.count", 12)`
- 语言代号从文件里自动发现（增删语言不用改代码），缺失 key 有记录
- 也可运行时注册（把翻译编进二进制），或从内存 JSON 加载
- 线程安全

## 依赖

| 依赖 | 说明 |
| --- | --- |
| `format/StrFormat.h` | 同仓库，header-only |
| [nlohmann/json](https://github.com/nlohmann/json) v3.12.0 | `third_party/json` 子模块，header-only |

## 语言表格式

```json
{
  // 允许 // 与 /* */ 注释
  "app.name": {
    "en": "JiaoZiPi",
    "zh": "饺子皮",
    "ja": "餃子の皮"
  },
  "menu.library": { "en": "Library", "zh": "游戏库" },
  "library.count": { "en": "{} games", "zh": "共 {} 个游戏" }
}
```

| 规则 | 说明 |
| --- | --- |
| key | 点分扁平写法（`menu.library`），一个 key 一条 |
| 值 | 必须是 `{ 语言代号: 文本 }` 对象；值不是对象的条目会被忽略 |
| 语言代号 | 自由取名（`en` / `zh` / `zh_CN` / `ja` ...），从文件里自动汇总 |
| 缺语言 | 某个 key 少了某语言时按查找链回退，不会报错 |
| 非字符串 | 数字、布尔转成文本；对象、数组忽略 |
| 失败处理 | JSON 非法或根节点不是对象 → `LoadFile` 返回 false，原因见 `LastError()` |

## 快速开始

```cpp
#include "i18n/I18n.h"

i18n::Translator& tr = i18n::Translator::Default();
tr.SetFallbackLocale("en");
if (!tr.LoadFile("resources/lang/language.json")) {
    LOG_ERROR("语言表加载失败：{}", tr.LastError());
}
tr.SetLocale("zh");                           // 返回 false 表示该语言没有条目（仍会切换并回退）

std::string title = tr.Tr("app.name");        // 饺子皮
std::string msg   = tr.Tr("library.count", 12);  // 共 12 个游戏

std::vector<std::string> locales = tr.Locales();  // {"en", "ja", "zh"}

// 全局便捷接口（走 Default()）
i18n::SetLocale("ja");
i18n::Tr("menu.settings");
```

内置语言表（无文件系统时）：

```cpp
tr.Register("zh", {
    {"app.name", "饺子皮"},
    {"menu.quit", "退出"},
});

// 或直接吃一段 JSON 文本
tr.LoadString(R"({"app.name": {"ja": "餃子の皮"}})");
```

排查漏翻译：

```cpp
for (const auto& key : tr.MissingKeys()) {
    LOG_WARN("缺少翻译：{}", key);
}
```

## 迁移到其他项目

1. 复制 `format/` 与 `i18n/` 两个目录；
2. 头文件搜索路径加入 `i18n/`、`format/` 的上级目录与 `third_party/json/include`；
3. 把 `I18n.cpp` 加进构建。

```bash
c++ -std=c++17 -I src/utils -I third_party/json/include \
    src/utils/i18n/I18n.cpp your_app.cpp -o your_app
```

## 注意

- 语言表是单个大 JSON，启动时一次性读入内存；文件很大时可换成 `Register()` 内置表或拆分后多次 `LoadFile`（后者会按语言合并）。
- nlohmann/json 是重量级模板库，会明显增加 `I18n.cpp` 的编译时间；只影响该 .cpp，包含 `I18n.h` 的业务文件不受影响。
- 字符串统一按 UTF-8 处理，渲染字体需自行支持对应字形。
