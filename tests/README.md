# tests —— 测试

| 文件 | 说明 |
| --- | --- |
| [utils_smoke.cpp](utils_smoke.cpp) | `src/utils/` 日志与多语言模块冒烟测试 |

```bash
git submodule update --init --recursive   # 首次：拉取 spdlog / nlohmann-json
sh tools/build_utils_test.sh              # 构建并运行，需在仓库根目录
```

脚本展开的编译命令：

```bash
c++ -std=c++17 -Wall -Wextra -O1 \
    -I src/utils -I third_party/spdlog/include -I third_party/json/include \
    src/utils/log/Logger.cpp src/utils/i18n/I18n.cpp tests/utils_smoke.cpp -o tests/utils_smoke
./tests/utils_smoke
```

约定：

- 新增模块自带冒烟测试，保持零框架依赖、可直接编译
- 引入测试框架前不写依赖框架的用例；测试产生的临时文件写在本目录下并自行清理
- `tests/utils_smoke`（可执行产物）已在 `.gitignore` 中忽略
