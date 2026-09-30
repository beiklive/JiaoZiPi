# tests —— 测试

| 文件 | 说明 |
| --- | --- |
| [utils_smoke.cpp](utils_smoke.cpp) | `src/utils/` 日志与多语言模块冒烟测试，无第三方依赖 |

```bash
c++ -std=c++17 -Wall -Wextra -I src/utils \
    src/utils/log/Logger.cpp src/utils/i18n/I18n.cpp tests/utils_smoke.cpp -o tests/utils_smoke
./tests/utils_smoke        # 需在仓库根目录运行（按 resources/lang 相对路径加载语言文件）
```

约定：

- 新增模块自带冒烟测试，保持零依赖、可直接编译
- 引入测试框架前不写依赖框架的用例；测试产生的临时文件写在本目录下并自行清理
