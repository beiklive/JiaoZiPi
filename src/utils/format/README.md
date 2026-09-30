# format —— 字符串格式化

`StrFormat.h`，header-only、C++17、无第三方依赖。

```cpp
#include "format/StrFormat.h"

std::string text = strfmt::Format("核心 {} 数量 {}", "GBA", 2);
```

- `{}` 顺序占位；`{{` / `}}` 输出字面量花括号
- 参数不足时保留 `{}` 原文，便于发现漏传
- 被 [log/](../log/README.md) 与 [i18n/](../i18n/README.md) 共用

迁移：复制本目录，头文件搜索路径指向其上级目录即可。
