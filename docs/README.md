# docs —— 文档与预览

| 文件 | 说明 |
| --- | --- |
| [data_paths.md](data_paths.md) | 各平台数据目录规范：根路径、子目录布局、只读资源与可写数据、失败降级 |
| [badge_colors.png](badge_colors.png) | 机种主题色预览（PNG 列表） |
| [badge_colors.html](badge_colors.html) | 机种主题色展示页（卡片、游戏列表标签、选中态） |

两者由 `python3 tools/gen_badge_color_preview.py` 解析 [JiaoZiPiMachine.h](../src/utils/JiaoZiPiMachine.h) 生成，改色后重跑脚本即可同步。

约定：设计规范、接口说明、平台适配记录都放本目录，根 README 只保留总览。
