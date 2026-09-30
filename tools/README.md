# tools —— 辅助脚本

| 脚本 | 说明 |
| --- | --- |
| [gen_badge_color_preview.py](gen_badge_color_preview.py) | 解析 `src/utils/JiaoZiPiMachine.h`，生成 `docs/badge_colors.png` 与 `docs/badge_colors.html` |

```bash
python3 tools/gen_badge_color_preview.py
```

约定：

- 仅依赖 Python 标准库，避免为脚本额外引入依赖
- 构建脚本（CMake / 打包）等平台与构建系统确定后加入本目录
