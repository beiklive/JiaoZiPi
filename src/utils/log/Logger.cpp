#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>

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
    std::string date;      // YYYY-MM-DD
    std::string time;      // HH:MM:SS.mmm
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
// ConsoleSink
// ---------------------------------------------------------------------------
ConsoleSink::ConsoleSink() : ConsoleSink(Options()) {}

ConsoleSink::ConsoleSink(Options options) : options_(std::move(options)) {}

void ConsoleSink::Write(const Record& record, std::string_view line) {
    std::FILE* out = options_.stream;
    if (out == nullptr) {
        out = (options_.splitStreams && record.level >= Level::Error) ? stderr : stdout;
    }
    if (out == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (options_.color) {
        std::fputs(LevelColor(record.level), out);
    }
    std::fwrite(line.data(), 1, line.size(), out);
    if (options_.color) {
        std::fputs("\x1b[0m", out);
    }
    std::fputc('\n', out);
    if (options_.flushEveryWrite) {
        std::fflush(out);
    }
}

void ConsoleSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::FILE* out = options_.stream;
    if (out == nullptr) {
        out = stdout;
    }
    std::fflush(out);
    if (options_.stream == nullptr) {
        std::fflush(stderr);
    }
}

// ---------------------------------------------------------------------------
// FileSink
// ---------------------------------------------------------------------------
FileSink::FileSink(Options options) : options_(std::move(options)) {
    std::lock_guard<std::mutex> lock(mutex_);
    OpenLocked();
}

FileSink::~FileSink() {
    std::lock_guard<std::mutex> lock(mutex_);
    CloseLocked();
}

void FileSink::CloseLocked() {
    if (stream_ != nullptr) {
        std::fflush(stream_);
        std::fclose(stream_);
        stream_ = nullptr;
    }
}

void FileSink::OpenLocked() {
    CloseLocked();
    if (options_.path.empty()) {
        return;
    }
    stream_ = std::fopen(options_.path.c_str(), options_.append ? "ab" : "wb");
    if (stream_ == nullptr) {
        return;
    }
    size_ = 0;
    if (options_.append) {
        if (std::fseek(stream_, 0, SEEK_END) == 0) {
            const long position = std::ftell(stream_);
            size_ = position > 0 ? static_cast<std::size_t>(position) : 0;
        }
    }
    if (options_.flushEveryWrite) {
        std::fflush(stream_);
    }
}

void FileSink::RotateLocked() {
    CloseLocked();
    const std::string& path = options_.path;
    const int maxFiles = options_.maxFiles > 0 ? options_.maxFiles : 0;
    if (maxFiles > 0) {
        std::remove((path + "." + std::to_string(maxFiles)).c_str());
        for (int i = maxFiles - 1; i >= 1; --i) {
            const std::string from = path + "." + std::to_string(i);
            const std::string to = path + "." + std::to_string(i + 1);
            std::remove(to.c_str());
            std::rename(from.c_str(), to.c_str());
        }
        const std::string to = path + ".1";
        std::remove(to.c_str());
        std::rename(path.c_str(), to.c_str());
    } else {
        std::remove(path.c_str());
    }
    OpenLocked();
}

bool FileSink::IsOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stream_ != nullptr;
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
}

void FileSink::Write(const Record& /*record*/, std::string_view line) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_ == nullptr) {
        OpenLocked();
        if (stream_ == nullptr) {
            return;
        }
    }
    const std::size_t need = line.size() + 1;
    if (options_.maxBytes > 0 && size_ > 0 && size_ + need > options_.maxBytes) {
        RotateLocked();
        if (stream_ == nullptr) {
            return;
        }
    }
    std::fwrite(line.data(), 1, line.size(), stream_);
    std::fputc('\n', stream_);
    size_ += need;
    if (options_.flushEveryWrite) {
        std::fflush(stream_);  // 实时写入：进程崩溃也不丢日志
    }
}

void FileSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_ != nullptr) {
        std::fflush(stream_);
    }
}

// ---------------------------------------------------------------------------
// CallbackSink
// ---------------------------------------------------------------------------
CallbackSink::CallbackSink(Callback callback) : callback_(std::move(callback)) {}

void CallbackSink::Write(const Record& record, std::string_view line) {
    if (callback_) {
        callback_(record, line);
    }
}

// ---------------------------------------------------------------------------
// Logger
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
