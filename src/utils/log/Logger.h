#pragma once

// 通用日志系统（独立模块，不依赖宿主项目）
//
// - 等级过滤：Trace / Debug / Info / Warn / Error / Fatal / Off
// - 输出端：控制台（可着色）、文件（实时 flush、可选按大小轮转）、回调（游戏内覆盖层等）
// - 线程安全；C++17；仅依赖同仓库的 format/StrFormat.h（header-only）
//
// 迁移到其他项目：复制 log/ 与 format/ 两个目录，把它们的上级目录加入头文件搜索路径
// （例如 -Isrc/utils），并把 Logger.cpp 加进构建即可，无需任何宿主类型。

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "format/StrFormat.h"

namespace logging {

enum class Level : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5,
    Off = 6,
};

const char* LevelName(Level level);
const char* LevelColor(Level level);  // ANSI 前景色，Off 返回空串
bool ParseLevel(std::string_view text, Level& out);

struct Record {
    Level level = Level::Info;
    std::int64_t timestampMs = 0;  // Unix epoch 毫秒
    const char* file = nullptr;    // 可为 nullptr
    int line = 0;
    std::string message;
};

class Sink {
public:
    virtual ~Sink() = default;
    virtual void Write(const Record& record, std::string_view line) = 0;
    virtual void Flush() {}
};

// 控制台输出：默认 <= Warn 走 stdout，>= Error 走 stderr
class ConsoleSink final : public Sink {
public:
    struct Options {
        std::FILE* stream = nullptr;     // 指定后忽略 splitStreams
        bool splitStreams = true;
        bool color = true;
        bool flushEveryWrite = false;
    };

    ConsoleSink();
    explicit ConsoleSink(Options options);

    void Write(const Record& record, std::string_view line) override;
    void Flush() override;

private:
    Options options_;
    std::mutex mutex_;
};

// 文件输出：默认每次写入即 flush（实时落盘）
class FileSink final : public Sink {
public:
    struct Options {
        std::string path;
        bool append = true;
        bool flushEveryWrite = true;  // 实时写入
        std::size_t maxBytes = 0;     // > 0 时按大小轮转
        int maxFiles = 3;             // 轮转保留的备份数量（path.1 ... path.N）
    };

    explicit FileSink(Options options);
    ~FileSink() override;

    bool IsOpen() const;
    std::string Path() const;

    void SetFlushEveryWrite(bool value);
    void SetMaxBytes(std::size_t bytes, int maxFiles = 3);

    void Write(const Record& record, std::string_view line) override;
    void Flush() override;

private:
    void OpenLocked();
    void RotateLocked();
    void CloseLocked();

    Options options_;
    mutable std::mutex mutex_;
    std::FILE* stream_ = nullptr;
    std::size_t size_ = 0;
};

// 回调输出：供游戏内日志窗口之类的场景直接消费结构化记录
// 注意：回调在写日志的线程上同步执行，实现需自行保证线程安全与耗时可控
class CallbackSink final : public Sink {
public:
    using Callback = std::function<void(const Record&, std::string_view)>;

    explicit CallbackSink(Callback callback);

    void Write(const Record& record, std::string_view line) override;

private:
    Callback callback_;
};

class Logger {
public:
    struct Options {
        Level level = Level::Info;
        // 可用占位符：{datetime} {date} {time} {level} {file} {line} {message}
        std::string pattern = "[{time}] [{level}] {file}:{line} {message}";
        bool utc = false;       // 时间戳是否使用 UTC
        bool shortFile = true;  // {file} 只输出文件名
    };

    Logger();
    explicit Logger(Options options);

    void SetLevel(Level level);
    Level GetLevel() const;
    bool ShouldLog(Level level) const;

    void SetPattern(std::string pattern);
    void SetUtc(bool utc);
    void SetShortFile(bool shortFile);

    void AddSink(std::shared_ptr<Sink> sink);
    void RemoveAllSinks();
    std::size_t SinkCount() const;

    // file/line 可为 nullptr/0（宏入口会传入真实位置）
    void Log(Level level, const char* file, int line, std::string message);
    void Log(Level level, std::string message);

    void Flush();

    static Logger& Default();

private:
    std::string FormatLine(const Record& record) const;

    std::atomic<int> level_;
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    std::string pattern_;
    bool utc_;
    bool shortFile_;
};

// 便捷构造：创建并挂载输出端，返回指针便于后续调整
std::shared_ptr<ConsoleSink> AddConsoleSink(Logger& logger, bool color = true, bool flushEveryWrite = false);
std::shared_ptr<FileSink> AddFileSink(Logger& logger, const FileSink::Options& options);

// 一行完成默认日志器初始化（清空既有输出端）
// logFile 为空表示不写文件
void InitDefault(Level level, std::string_view logFile = std::string_view(), bool console = true,
                 bool color = true, bool flushEveryWrite = true);

namespace detail {

template <class... Args>
inline void LogDefault(Level level, const char* file, int line, std::string_view format, Args&&... args) {
    Logger& logger = Logger::Default();
    if (!logger.ShouldLog(level)) {
        return;
    }
    logger.Log(level, file, line, strfmt::Format(format, std::forward<Args>(args)...));
}

template <class... Args>
inline void LogTo(Logger& logger, Level level, const char* file, int line, std::string_view format,
                  Args&&... args) {
    if (!logger.ShouldLog(level)) {
        return;
    }
    logger.Log(level, file, line, strfmt::Format(format, std::forward<Args>(args)...));
}

}  // namespace detail

}  // namespace logging

// 全局默认日志器
#define LOG_TRACE(...) \
    ::logging::detail::LogDefault(::logging::Level::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) \
    ::logging::detail::LogDefault(::logging::Level::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) ::logging::detail::LogDefault(::logging::Level::Info, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) ::logging::detail::LogDefault(::logging::Level::Warn, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) \
    ::logging::detail::LogDefault(::logging::Level::Error, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) \
    ::logging::detail::LogDefault(::logging::Level::Fatal, __FILE__, __LINE__, __VA_ARGS__)

// 指定日志器
#define LOG_TRACE_TO(logger, ...) \
    ::logging::detail::LogTo((logger), ::logging::Level::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG_TO(logger, ...) \
    ::logging::detail::LogTo((logger), ::logging::Level::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO_TO(logger, ...) \
    ::logging::detail::LogTo((logger), ::logging::Level::Info, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN_TO(logger, ...) \
    ::logging::detail::LogTo((logger), ::logging::Level::Warn, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR_TO(logger, ...) \
    ::logging::detail::LogTo((logger), ::logging::Level::Error, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL_TO(logger, ...) \
    ::logging::detail::LogTo((logger), ::logging::Level::Fatal, __FILE__, __LINE__, __VA_ARGS__)
