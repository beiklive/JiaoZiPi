# tools —— 辅助脚本

| 脚本 | 说明 |
| --- | --- |
| [gen_badge_color_preview.py](gen_badge_color_preview.py) | 解析 `src/utils/JiaoZiPiMachine.h`，生成 `docs/badge_colors.png` 与 `docs/badge_colors.html` |
| [gen_platform_configs.py](gen_platform_configs.py) | 解析机种 key / 名称，生成 `resources/platforms/*.json`（含核心名与后缀列表） |
| [build_utils_test.sh](build_utils_test.sh) | 构建并运行 `tests/utils_smoke.cpp`（自动带上 spdlog / nlohmann-json 的 include 路径） |

```bash
python3 tools/gen_badge_color_preview.py   # 配色预览
python3 tools/gen_platform_configs.py      # 各机种内置配置
sh tools/build_utils_test.sh               # 工具模块冒烟测试
```

约定：

- 预览脚本仅依赖 Python 标准库，避免为脚本额外引入依赖
- 构建脚本（CMake / 打包）等平台与构建系统确定后加入本目录
