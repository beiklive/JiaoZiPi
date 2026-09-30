// 日志系统实现：IO、轮转、控制台着色由 spdlog 承担（third_party/spdlog），
// 本文件只保留等级过滤、行格式与输出端门面（Sink 接口）。
// spdlog 以 header-only 方式使用，无需额外编译 spdlog 源码。

#define SPDLOG_HEADER_ONLY

#include "Logger.h"

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>

namespace logging {
namespace {

constexpr const char* kLevelNames[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"};
constexpr const char* kLevelColors[] = {"\x1b[90m", "\x1b[36m", "\x1b[32m",
                                        "\x1b[33m", "\x1b[31m", "\x1b[1;31m", ""};

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string ToLower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string BaseName(const char* path) {
    if (path == nullptr) {
        return "-";
    }
    std::string_view view(path);
    const std::size_t slash = view.find_last_of("/\\");
    return std::string(slash == std::string_view::npos ? view : view.substr(slash + 1));
}

struct TimeParts {
    std::string date;  // YYYY-MM-DD
    std::string time;  // HH:MM:SS.mmm
};

TimeParts SplitTime(std::int64_t timestampMs, bool utc) {
    const std::time_t seconds = static_cast<std::time_t>(timestampMs / 1000);
    std::tm tm{};
#if defined(_WIN32)
    if (utc) {
        gmtime_s(&tm, &seconds);
    } else {
        localtime_s(&tm, &seconds);
    }
#else
    if (utc) {
        gmtime_r(&seconds, &tm);
    } else {
        localtime_r(&seconds, &tm);
    }
#endif

    char date[16] = {};
    char time[16] = {};
    std::snprintf(date, sizeof(date), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    std::snprintf(time, sizeof(time), "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(timestampMs % 1000));
    return TimeParts{date, time};
}

// 把本模块等级映射到 spdlog 等级（spdlog 只用于着色与输出）
spdlog::level::level_enum ToSpdlogLevel(Level level) {
    switch (level) {
        case Level::Trace: return spdlog::level::trace;
        case Level::Debug: return spdlog::level::debug;
        case Level::Info: return spdlog::level::info;
        case Level::Warn: return spdlog::level::warn;
        case Level::Error: return spdlog::level::err;
        case Level::Fatal: return spdlog::level::critical;
        case Level::Off:
        default: return spdlog::level::off;
    }
}

// 本模块已完成整行格式化，交给 spdlog 的 sink 时只用 %v（着色时加 %^ %$ 让 sink 按等级上色）
constexpr const char* kPlainPattern = "%v";
constexpr const char* kColorPattern = "%^%v%$";

spdlog::details::log_msg MakeMessage(const Record& record, std::string_view line) {
    return spdlog::details::log_msg{spdlog::log_clock::now(),
                                    spdlog::source_loc{record.file, record.line, ""},
                                    spdlog::string_view_t{"jzp", 3}, ToSpdlogLevel(record.level),
                                    spdlog::string_view_t{line.data(), line.size()}};
}

}  // namespace

const char* LevelName(Level level) {
    const int index = static_cast<int>(level);
    if (index < 0 || index >= static_cast<int>(sizeof(kLevelNames) / sizeof(kLevelNames[0]))) {
        return "UNKNOWN";
    }
    return kLevelNames[index];
}

const char* LevelColor(Level level) {
    const int index = static_cast<int>(level);
    if (index < 0 || index >= static_cast<int>(sizeof(kLevelColors) / sizeof(kLevelColors[0]))) {
        return "";
    }
    return kLevelColors[index];
}

bool ParseLevel(std::string_view text, Level& out) {
    const std::string lowered = ToLower(text);
    for (int i = 0; i < static_cast<int>(sizeof(kLevelNames) / sizeof(kLevelNames[0])); ++i) {
        if (lowered == ToLower(kLevelNames[i])) {
            out = static_cast<Level>(i);
            return true;
        }
    }
    if (lowered == "warning") {
        out = Level::Warn;
        return true;
    }
    if (lowered == "err") {
        out = Level::Error;
        return true;
    }
    if (lowered == "none" || lowered == "silent") {
        out = Level::Off;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ConsoleSink —— 底层为 spdlog 的 stdout/stderr 彩色 sink
// ---------------------------------------------------------------------------
ConsoleSink::ConsoleSink() : ConsoleSink(Options()) {}

ConsoleSink::ConsoleSink(Options options) : options_(std::move(options)) {
    try {
        if (options_.color) {
            out_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            if (options_.splitStreams) {
                err_ = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            }
        } else {
            out_ = std::make_shared<spdlog::sinks::stdout_sink_mt>();
            if (options_.splitStreams) {
                err_ = std::make_shared<spdlog::sinks::stderr_sink_mt>();
            }
        }
    } catch (const std::exception&) {
        out_.reset();
        err_.reset();
        return;
    }

    // 行内容已由本模块格式化，sink 只输出 %v；着色时用 %^%v%$ 让 sink 按等级上色
    for (const auto& sink : {out_, err_}) {
        if (sink != nullptr) {
            sink->set_pattern(options_.color ? kColorPattern : kPlainPattern);
            sink->set_level(spdlog::level::trace);
        }
    }
}

void ConsoleSink::Write(const Record& record, std::string_view line) {
    auto* sink = (options_.splitStreams && record.level >= Level::Error && err_ != nullptr)
                     ? err_.get()
                     : out_.get();
    if (sink == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        sink->log(MakeMessage(record, line));
        if (options_.flushEveryWrite) {
            sink->flush();
        }
    } catch (const std::exception&) {
        // 输出失败不影响主流程
    }
}

void ConsoleSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& sink : {out_, err_}) {
        if (sink != nullptr) {
            try {
                sink->flush();
            } catch (const std::exception&) {
            }
        }
    }
}

// ---------------------------------------------------------------------------
// FileSink —— 底层为 spdlog 的 basic_file_sink / rotating_file_sink
// ---------------------------------------------------------------------------
FileSink::FileSink(Options options) : options_(std::move(options)) {
    std::lock_guard<std::mutex> lock(mutex_);
    OpenLocked();
}

FileSink::~FileSink() = default;

void FileSink::OpenLocked() {
    sink_.reset();
    open_ = false;
    if (options_.path.empty()) {
        return;
    }
    try {
        if (options_.maxBytes > 0) {
            sink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                options_.path, options_.maxBytes,
                static_cast<std::size_t>(options_.maxFiles > 0 ? options_.maxFiles : 1), false);
        } else {
            sink_ = std::make_shared<spdlog::sinks::basic_file_sink_mt>(options_.path, !options_.append);
        }
        sink_->set_pattern(kPlainPattern);
        sink_->set_level(spdlog::level::trace);
        open_ = true;
        if (options_.flushEveryWrite) {
            sink_->flush();
        }
    } catch (const std::exception&) {
        sink_.reset();
        open_ = false;
    }
}

bool FileSink::IsOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return open_;
}

std::string FileSink::Path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return options_.path;
}

void FileSink::SetFlushEveryWrite(bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    options_.flushEveryWrite = value;
}

void FileSink::SetMaxBytes(std::size_t bytes, int maxFiles) {
    std::lock_guard<std::mutex> lock(mutex_);
    options_.maxBytes = bytes;
    options_.maxFiles = maxFiles > 0 ? maxFiles : 1;
    OpenLocked();  // 轮转与不轮转由不同的 spdlog sink 实现，切换时重建
}

void FileSink::Write(const Record& record, std::string_view line) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sink_ == nullptr) {
        OpenLocked();
        if (sink_ == nullptr) {
            return;
        }
    }
    try {
        sink_->log(MakeMessage(record, line));
        if (options_.flushEveryWrite) {
            sink_->flush();  // 实时写入：进程崩溃也不丢日志
        }
    } catch (const std::exception&) {
        open_ = false;
    }
}

void FileSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sink_ != nullptr) {
        try {
            sink_->flush();
        } catch (const std::exception&) {
        }
    }
}

// ---------------------------------------------------------------------------
// CallbackSink —— 直接回调，供游戏内日志窗口消费结构化记录
// ---------------------------------------------------------------------------
CallbackSink::CallbackSink(Callback callback) : callback_(std::move(callback)) {}

void CallbackSink::Write(const Record& record, std::string_view line) {
    if (callback_) {
        callback_(record, line);
    }
}

// ---------------------------------------------------------------------------
// Logger：等级过滤 + 行格式 + 输出端分发
// ---------------------------------------------------------------------------
Logger::Logger() : Logger(Options()) {}

Logger::Logger(Options options)
    : level_(static_cast<int>(options.level)),
      pattern_(std::move(options.pattern)),
      utc_(options.utc),
      shortFile_(options.shortFile) {}

void Logger::SetLevel(Level level) { level_.store(static_cast<int>(level)); }

Level Logger::GetLevel() const { return static_cast<Level>(level_.load()); }

bool Logger::ShouldLog(Level level) const {
    const Level current = GetLevel();
    if (level == Level::Off || current == Level::Off) {
        return false;
    }
    return static_cast<int>(level) >= static_cast<int>(current);
}

void Logger::SetPattern(std::string pattern) {
    std::lock_guard<std::mutex> lock(mutex_);
    pattern_ = std::move(pattern);
}

void Logger::SetUtc(bool utc) {
    std::lock_guard<std::mutex> lock(mutex_);
    utc_ = utc;
}

void Logger::SetShortFile(bool shortFile) {
    std::lock_guard<std::mutex> lock(mutex_);
    shortFile_ = shortFile;
}

void Logger::AddSink(std::shared_ptr<Sink> sink) {
    if (sink == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::RemoveAllSinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

std::size_t Logger::SinkCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sinks_.size();
}

std::string Logger::FormatLine(const Record& record) const {
    std::string pattern;
    bool utc = false;
    bool shortFile = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pattern = pattern_;
        utc = utc_;
        shortFile = shortFile_;
    }

    const TimeParts parts = SplitTime(record.timestampMs, utc);
    const std::string file = record.file == nullptr
                                 ? std::string("-")
                                 : (shortFile ? BaseName(record.file) : std::string(record.file));
    const std::string line = record.line > 0 ? std::to_string(record.line) : std::string("0");
    const std::string datetime = parts.date + " " + parts.time;

    std::string out;
    out.reserve(pattern.size() + record.message.size() + 64);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] != '{') {
            out.push_back(pattern[i]);
            continue;
        }
        const std::string_view rest(pattern.data() + i, pattern.size() - i);
        if (rest.compare(0, 11, "{datetime}") == 0) {
            out += datetime;
            i += 10;
        } else if (rest.compare(0, 6, "{date}") == 0) {
            out += parts.date;
            i += 5;
        } else if (rest.compare(0, 6, "{time}") == 0) {
            out += parts.time;
            i += 5;
        } else if (rest.compare(0, 7, "{level}") == 0) {
            out += LevelName(record.level);
            i += 6;
        } else if (rest.compare(0, 6, "{file}") == 0) {
            out += file;
            i += 5;
        } else if (rest.compare(0, 6, "{line}") == 0) {
            out += line;
            i += 5;
        } else if (rest.compare(0, 9, "{message}") == 0) {
            out += record.message;
            i += 8;
        } else {
            out.push_back(pattern[i]);
        }
    }
    return out;
}

void Logger::Log(Level level, const char* file, int line, std::string message) {
    if (!ShouldLog(level)) {
        return;
    }

    Record record;
    record.level = level;
    record.timestampMs = NowMs();
    record.file = file;
    record.line = line;
    record.message = std::move(message);

    std::vector<std::shared_ptr<Sink>> sinks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks = sinks_;
    }
    if (sinks.empty()) {
        return;
    }

    const std::string text = FormatLine(record);
    for (const auto& sink : sinks) {
        if (sink != nullptr) {
            sink->Write(record, text);
        }
    }
}

void Logger::Log(Level level, std::string message) { Log(level, nullptr, 0, std::move(message)); }

void Logger::Flush() {
    std::vector<std::shared_ptr<Sink>> sinks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks = sinks_;
    }
    for (const auto& sink : sinks) {
        if (sink != nullptr) {
            sink->Flush();
        }
    }
}

Logger& Logger::Default() {
    static Logger instance;
    return instance;
}

std::shared_ptr<ConsoleSink> AddConsoleSink(Logger& logger, bool color, bool flushEveryWrite) {
    ConsoleSink::Options options;
    options.color = color;
    options.flushEveryWrite = flushEveryWrite;
    auto sink = std::make_shared<ConsoleSink>(options);
    logger.AddSink(sink);
    return sink;
}

std::shared_ptr<FileSink> AddFileSink(Logger& logger, const FileSink::Options& options) {
    auto sink = std::make_shared<FileSink>(options);
    logger.AddSink(sink);
    return sink;
}

void InitDefault(Level level, std::string_view logFile, bool console, bool color, bool flushEveryWrite) {
    Logger& logger = Logger::Default();
    logger.RemoveAllSinks();
    logger.SetLevel(level);
    if (console) {
        AddConsoleSink(logger, color, false);
    }
    if (!logFile.empty()) {
        FileSink::Options options;
        options.path = std::string(logFile);
        options.append = true;
        options.flushEveryWrite = flushEveryWrite;
        AddFileSink(logger, options);
    }
}

}  // namespace logging
