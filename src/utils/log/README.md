# log —— 通用日志系统

独立模块，不依赖任何宿主项目代码，可直接复制到其他 C++17 工程。

## 特性

- 等级过滤：`Trace / Debug / Info / Warn / Error / Fatal / Off`，运行期可改
- 输出端（可任意组合，线程安全）：
  - `ConsoleSink`：控制台，可按等级分 stdout/stderr，可 ANSI 着色
  - `FileSink`：文件，**默认每条日志立即 flush（实时写入）**，可按大小轮转
  - `CallbackSink`：回调，直接拿结构化 `Record`（适合游戏内日志窗口）
- `{}` 占位符格式化（`format/StrFormat.h`），无 printf 格式串类型不匹配风险
- 自定义行格式：`{datetime} {date} {time} {level} {file} {line} {message}`

## 依赖

仅同仓库的 `format/StrFormat.h`（header-only）。`log/` 与 `i18n/` 互不依赖。

## 快速开始

```cpp
#include "log/Logger.h"

int main() {
    logging::InitDefault(logging::Level::Info, "logs/app.log");  // 控制台 + 文件（实时）
    LOG_INFO("启动完成，核心数量 {}", 15);
    LOG_ERROR("核心加载失败：{}", "mgba");

    logging::Logger& logger = logging::Logger::Default();
    logger.SetLevel(logging::Level::Debug);                      // 运行期改等级
    logger.SetPattern("[{datetime}] [{level}] {file}:{line} {message}");
}
```

自定义输出端：

```cpp
logging::Logger logger;
logger.SetLevel(logging::Level::Trace);

logging::ConsoleSink::Options console;
console.color = true;
logger.AddSink(std::make_shared<logging::ConsoleSink>(console));

logging::FileSink::Options file;
file.path = "logs/app.log";
file.append = true;
file.flushEveryWrite = true;   // 实时写入
file.maxBytes = 2 * 1024 * 1024;
file.maxFiles = 3;             // app.log.1 / .2 / .3
logger.AddSink(std::make_shared<logging::FileSink>(file));

logger.AddSink(std::make_shared<logging::CallbackSink>(
    [](const logging::Record& r, std::string_view line) {
        // 画到游戏内覆盖层：r.level 决定颜色，line 是格式化好的整行
    }));
```

宏：`LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL(...)` 走全局默认日志器；
`LOG_*_TO(logger, ...)` 走指定日志器。低于当前等级的调用不会做格式化与写入。

等级名可直接从配置读取：

```cpp
logging::Level level = logging::Level::Info;
logging::ParseLevel("warn", level);   // true, level == Warn
logger.SetLevel(level);
```

## 迁移到其他项目

1. 复制 `format/` 与 `log/` 两个目录；
2. 头文件搜索路径指向它们的上级目录（例如 `-I<root>/utils`）；
3. 把 `Logger.cpp` 加入构建。

没有 CMake 集成时可直接：

```bash
c++ -std=c++17 -I src/utils src/utils/log/Logger.cpp your_app.cpp -o your_app
```

## 注意

- `CallbackSink` 在写日志的线程上同步执行，回调实现要快且线程安全。
- 控制台 ANSI 着色在 Windows 需要先启用 VT 模式；不支持时把 `color` 设为 `false`。
- 想换成 spdlog/quill 等三方库时，只需用适配的 `Sink` 实现替换 `FileSink`，调用点（宏与 `Logger` 接口）不用动。
