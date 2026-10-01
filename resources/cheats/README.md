# cheats —— 金手指

放什么：随包发布的内置金手指，格式为 RetroArch 兼容的 `.cht`。

```text
cheats/<机种>/<game-id>.cht          # 只读，随包发布
```

文件格式（key = value，非 JSON）：

```ini
cheats = 1

cheat0_desc = "无限生命"
cheat0_code = "82003208 0063"
cheat0_enable = false
cheat0_big_endian = false
```

约定：

- 目录按 [`MachineKey()`](../../src/utils/JiaoZiPiMachine.h) 分（`gba` / `ps1` / `3ds` …），文件名用稳定 game-id
- 直接用 RetroArch 社区的 `.cht` 文件即可，不必转换格式
- 用户自己的金手指放数据根目录下的同名结构，加载顺序「数据根 → 内置」
- 解析与读写由 [src/utils/cheats](../../src/utils/cheats/README.md) 负责，前端与核心都不需要懂代码串格式

完整字段说明见 [cheats 模块文档](../../src/utils/cheats/README.md)。
