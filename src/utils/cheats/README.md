# cheats —— 金手指 .cht 解析器

独立模块，不依赖任何宿主项目代码，可复制到其他 C++17 工程。格式来源：[RetroArch `cheat_manager.c`](https://github.com/libretro/RetroArch/blob/master/cheat_manager.c)（`.cht` 是 key = value 配置格式，**不是 JSON**）。

## 职责与边界

做：读取 / 修改 / 写回 `.cht`，保留未知字段。

不做（保持低耦合）：不建目录、不管路径、不解析核心代码串、不判断金手指是否生效。文件位置交给 [src/utils/paths](../paths/README.md)，代码串由核心解释。

## 格式

```ini
cheats = 2

cheat0_desc = "Infinite Health"
cheat0_code = "82003208 0063+8200320A 00FF"
cheat0_enable = false
cheat0_big_endian = false
cheat0_handler = 0            # 未知字段，默认原样保留
cheat0_repeat_count = 0

cheat1_desc = "Max Money"
cheat1_code = "AAAAAAAA-BBBBBBBB"
cheat1_enable = true
cheat1_big_endian = false
```

| 键 | 说明 |
| --- | --- |
| `cheats` | 条目总数 |
| `cheat<N>_desc` | 描述（空则写出 `code`，与 RA 行为一致） |
| `cheat<N>_code` | 核心原生代码串；多行用 `+` 连接，前端不解析 |
| `cheat<N>_enable` | `true` / `false`（读取时也接受 `1`） |
| `cheat<N>_big_endian` | `true` / `false` |
| 其它 `cheat<N>_*` | `handler` / `cheat_type` / `rumble_*` / `repeat_*` 等，**原样保留**，写回不丢 |

容错：UTF-8 BOM、CRLF、`#` / `;` 注释、空行、字段任意顺序、条目编号跳号、`cheats` 计数与实际不一致、超大计数声明（> 100000 忽略）。

## 用法

```cpp
#include "cheats/ChtFile.h"

cht::File file;
if (!file.Load(paths::DataPath("cheats/gba/abc123.cht"))) {
    LOG_WARN("金手指读取失败：{}", file.LastError());
}

for (const cht::Entry& entry : file.Entries()) {
    // entry.description / entry.code / entry.enabled / entry.bigEndian
}

file.SetEnabled(0, true);                    // 修改
file.SetCode(0, "82003208 0064");
file.SetDescription(0, "无限生命");

const std::size_t index = file.Add("Debug", "DDDDDDDD-EEEEEEEE", true);
file.Remove(index);
file.EnableAll(false);

file.Save(path);                             // 写回（父目录需已存在）
std::string text = file.ToString();          // 或自己写到别处
```

## API

| 分类 | 接口 |
| --- | --- |
| 读写 | `Load(path)`、`LoadFromString(text)`、`Save(path)`、`ToString()` |
| 读取 | `Size()`、`Empty()`、`Entries()`、`Get(index)`（越界返回 `nullptr`）、`CountEnabled()`、`IndexOfCode(code)` |
| 修改 | `Add(entry)`、`Add(desc, code, enabled)`、`Remove(index)`、`Clear()`、`SetDescription/SetCode/SetEnabled/SetBigEndian(index, …)`、`EnableAll(bool)` |
| 诊断 | `LastError()`、`Warnings()`、`ClearWarnings()` |

## 迁移到其他项目

复制 `cheats/` 一个目录即可：

```bash
c++ -std=c++17 -I src/utils src/utils/cheats/ChtFile.cpp your_app.cpp -o your_app
```

## 注意

- 默认 `Options::preserveUnknownKeys = true`：无法识别的字段与文件级未知键都会原样写回。想让输出只保留 5 个标准键，用 `cht::File file{cht::File::Options{false}};`。
- 解析失败的行不会中断加载，而是进 `Warnings()`；`Load()` 只在文件打不开时返回 `false`。
- 空文件按「空列表」成功加载（同时给一条 warning）。
- 写回时固定输出 `\n`、无 BOM，`cheats` 计数按实际条目数重算。
- 不做原子写（直接覆盖），需要防写坏的场景请自行先写临时文件再 rename。
