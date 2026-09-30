# log —— 通用日志系统

独立模块，不依赖任何宿主项目代码，可复制到其他 C++17 工程。

## 特性

- 等级过滤：`Trace / Debug / Info / Warn / Error / Fatal / Off`，运行期可改
- 输出端（可任意组合，线程安全）：
  - `ConsoleSink`：控制台，可按等级分 stdout/stderr，终端下按等级着色
  - `FileSink`：文件，**默认每条日志立即 flush（实时写入）**，可按大小轮转
  - `CallbackSink`：回调，直接拿结构化 `Record`（适合游戏内日志窗口）
- `{}` 占位符格式化（`format/StrFormat.h`），无 printf 格式串类型不匹配风险
- 自定义行格式：`{datetime} {date} {time} {level} {file} {line} {message}`

## 依赖

| 依赖 | 说明 |
| --- | --- |
| `format/StrFormat.h` | 同仓库，header-only |
| [spdlog](https://github.com/gabime/spdlog) v1.17.0 | `third_party/spdlog` 子模块，**以 header-only 方式使用**（`Logger.cpp` 内定义 `SPDLOG_HEADER_ONLY`），无需编译 spdlog 源码 |

分工：spdlog 负责文件 IO、大小轮转、控制台着色与 flush；本模块负责等级过滤、行格式与 Sink 门面。
`Logger.h` 不暴露任何 spdlog 类型（只做前置声明），因此业务代码只 include 本头文件即可。

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
file.maxFiles = 3;             // app.1.log / app.2.log / app.3.log（spdlog 命名规则）
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

## 构建

```bash
c++ -std=c++17 -I src/utils -I third_party/spdlog/include \
    src/utils/log/Logger.cpp your_app.cpp -o your_app
```

## 迁移到其他项目

1. `git submodule update --init --recursive` 取回 `third_party/spdlog`；
2. 复制 `format/` 与 `log/` 两个目录；
3. 头文件搜索路径加入 `log/`、`format/` 的上级目录与 `third_party/spdlog/include`；
4. 把 `Logger.cpp` 加进构建。

## 注意

- `CallbackSink` 在写日志的线程上同步执行，回调实现要快且线程安全。
- 终端着色由 spdlog 判定：输出不是终端（重定向到管道/文件）时自动去色。
- `maxBytes > 0` 换成 spdlog 的 rotating sink，备份命名是 `app.1.log`（不是 `app.log.1`）。
- 只使用 spdlog 的 sink（不注册 logger、不使用异步线程池），因此不需要调用 `spdlog::init_thread_pool` 之类的初始化。
