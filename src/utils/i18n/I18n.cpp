#include "I18n.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace i18n {
namespace {

// ---------------------------------------------------------------------------
// 文件与字符串工具
// ---------------------------------------------------------------------------
bool ReadFile(const std::string& path, std::string& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    char buffer[4096];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        out.append(buffer, read);
    }
    std::fclose(file);
    return true;
}

std::string_view Trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

void WithoutBom(std::string& content) {
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
}

std::string JoinPath(std::string_view directory, std::string_view name) {
    std::string path(directory);
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path.push_back('/');
    }
    path.append(name.data(), name.size());
    return path;
}

bool HasExtension(std::string_view name, std::string_view extension) {
    if (extension.empty() || name.size() <= extension.size()) {
        return false;
    }
    return name.compare(name.size() - extension.size(), extension.size(), extension) == 0;
}

// 后缀参数接受逗号分隔的多个后缀，如 ".lang,.json"；为空表示不过滤
std::vector<std::string_view> SplitExtensions(std::string_view csv) {
    std::vector<std::string_view> out;
    std::size_t position = 0;
    while (position <= csv.size()) {
        std::size_t comma = csv.find(',', position);
        if (comma == std::string_view::npos) {
            comma = csv.size();
        }
        const std::string_view part = Trim(csv.substr(position, comma - position));
        if (!part.empty()) {
            out.push_back(part);
        }
        if (comma == csv.size()) {
            break;
        }
        position = comma + 1;
    }
    return out;
}

// ---------------------------------------------------------------------------
// 语言表解析（自制，行式扁平格式，兼容两种写法）
//   key = value          （文本表，原有写法）
//   "key": "value",      （JSON 风格写法，引号、逗号、花括号均可省略）
// 忽略：空行、# ; // 注释行、纯结构行 { } [ ] ,
// 支持转义：\n \t \r \b \f \" \/ \\ 以及 \uXXXX（BMP）
// 不支持：嵌套对象（请用点分 key 表达层级）、数组
// ---------------------------------------------------------------------------
void AppendUtf8(std::string& out, unsigned int code) {
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

unsigned int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<unsigned int>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<unsigned int>(c - 'a' + 10);
    }
    return static_cast<unsigned int>(c - 'A' + 10);
}

std::string Unescape(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != '\\' || i + 1 >= raw.size()) {
            out.push_back(raw[i]);
            continue;
        }
        const char esc = raw[++i];
        switch (esc) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case '"': out.push_back('"'); break;
            case '/': out.push_back('/'); break;
            case '\\': out.push_back('\\'); break;
            case 'u': {
                unsigned int code = 0;
                int digits = 0;
                while (digits < 4 && i + 1 < raw.size() &&
                       std::isxdigit(static_cast<unsigned char>(raw[i + 1])) != 0) {
                    code = code * 16 + HexValue(raw[++i]);
                    ++digits;
                }
                if (digits == 4) {
                    AppendUtf8(out, code);
                } else {
                    out += "\\u";
                }
                break;
            }
            default:
                out.push_back('\\');
                out.push_back(esc);
                break;
        }
    }
    return out;
}

// 读取带引号的片段：pos 指向开引号，返回是否闭合，结束时 pos 指向闭引号之后
bool ReadQuoted(std::string_view line, std::size_t& pos, std::string& out) {
    ++pos;  // 跳过开引号
    std::string raw;
    while (pos < line.size()) {
        const char ch = line[pos];
        if (ch == '\\' && pos + 1 < line.size()) {
            raw.push_back(ch);
            raw.push_back(line[pos + 1]);
            pos += 2;
            continue;
        }
        if (ch == '"') {
            ++pos;
            out = Unescape(raw);
            return true;
        }
        raw.push_back(ch);
        ++pos;
    }
    return false;
}

bool ParseEntry(std::string_view line, std::string& key, std::string& value) {
    key.clear();
    value.clear();

    line = Trim(line);
    if (line.empty()) {
        return false;
    }
    if (line.front() == '#' || line.front() == ';') {
        return false;
    }
    if (line.size() >= 2 && line[0] == '/' && line[1] == '/') {
        return false;
    }
    if (line.find_first_not_of("{}[],") == std::string_view::npos) {
        return false;  // 纯结构行
    }

    std::size_t pos = 0;
    if (line.front() == '"') {
        if (!ReadQuoted(line, pos, key)) {
            return false;
        }
    } else {
        const std::size_t separator = line.find_first_of("=:");
        if (separator == std::string_view::npos || separator == 0) {
            return false;
        }
        key = std::string(Trim(line.substr(0, separator)));
        pos = separator;
    }

    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) != 0) {
        ++pos;
    }
    if (pos >= line.size() || (line[pos] != '=' && line[pos] != ':')) {
        return false;
    }
    ++pos;
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) != 0) {
        ++pos;
    }
    if (pos >= line.size()) {
        return false;  // 空值
    }

    if (line[pos] == '"') {
        if (!ReadQuoted(line, pos, value)) {
            return false;
        }
    } else {
        std::string_view rest = Trim(line.substr(pos));
        if (!rest.empty() && (rest.front() == '{' || rest.front() == '[')) {
            return false;  // 嵌套结构不支持
        }
        if (!rest.empty() && rest.back() == ',') {
            rest.remove_suffix(1);
            rest = Trim(rest);
        }
        if (rest.empty()) {
            return false;
        }
        value = std::string(rest);
    }

    return !key.empty();
}

}  // namespace

Translator::Translator() : Translator(Options()) {}

Translator::Translator(Options options) : options_(std::move(options)) {
    if (options_.fallbackLocale.empty()) {
        options_.fallbackLocale = "en_US";
    }
    locale_ = options_.fallbackLocale;
}

std::string Translator::LanguagePart(std::string_view locale) {
    const std::size_t separator = locale.find_first_of("_-");
    return std::string(separator == std::string_view::npos ? locale : locale.substr(0, separator));
}

void Translator::Set(std::string_view locale, std::string_view key, std::string_view value) {
    if (locale.empty() || key.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    table_[std::string(locale)][std::string(key)] = std::string(value);
}

void Translator::Register(std::string_view locale, std::initializer_list<Entry> entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& target = table_[std::string(locale)];
    for (const Entry& entry : entries) {
        if (entry.key != nullptr && entry.value != nullptr) {
            target[entry.key] = entry.value;
        }
    }
}

bool Translator::LoadFile(std::string_view locale, std::string_view path) {
    std::string content;
    if (!ReadFile(std::string(path), content)) {
        return false;
    }
    WithoutBom(content);

    std::lock_guard<std::mutex> lock(mutex_);
    auto& target = table_[std::string(locale)];

    std::size_t position = 0;
    while (position <= content.size()) {
        const std::size_t newline = content.find('\n', position);
        const std::size_t end = newline == std::string::npos ? content.size() : newline;
        std::string_view line(content.data() + position, end - position);
        position = end + 1;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        std::string key;
        std::string value;
        if (ParseEntry(line, key, value)) {
            target[key] = std::move(value);
        }
        if (newline == std::string::npos) {
            break;
        }
    }
    return true;
}

std::size_t Translator::LoadDirectory(std::string_view directory, std::string_view extension) {
    const std::string dir(directory);
    const std::vector<std::string_view> extensions = SplitExtensions(extension);
    std::size_t loaded = 0;

#if defined(_WIN32)
    WIN32_FIND_DATAA data{};
    HANDLE handle = FindFirstFileA(JoinPath(dir, "*").c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        const std::string_view name(data.cFileName);
        std::string_view matched;
        for (const std::string_view candidate : extensions) {
            if (HasExtension(name, candidate)) {
                matched = candidate;
                break;
            }
        }
        if (!extensions.empty() && matched.empty()) {
            continue;
        }
        if (LoadFile(name.substr(0, name.size() - matched.size()), JoinPath(dir, name))) {
            ++loaded;
        }
    } while (FindNextFileA(handle, &data) != 0);
    FindClose(handle);
#else
    DIR* handle = opendir(dir.c_str());
    if (handle == nullptr) {
        return 0;
    }
    while (dirent* entry = readdir(handle)) {
        const std::string_view name(entry->d_name);
        if (name.empty() || name.front() == '.') {
            continue;  // 跳过 . / .. / 隐藏文件
        }
        std::string_view matched;
        for (const std::string_view candidate : extensions) {
            if (HasExtension(name, candidate)) {
                matched = candidate;
                break;
            }
        }
        if (!extensions.empty() && matched.empty()) {
            continue;
        }
        if (LoadFile(name.substr(0, name.size() - matched.size()), JoinPath(dir, name))) {
            ++loaded;
        }
    }
    closedir(handle);
#endif

    return loaded;
}

const std::string* Translator::FindLocked(std::string_view locale, std::string_view key) const {
    if (locale.empty()) {
        return nullptr;
    }
    const auto exact = table_.find(std::string(locale));
    if (exact != table_.end()) {
        const auto found = exact->second.find(std::string(key));
        if (found != exact->second.end()) {
            return &found->second;
        }
    }

    // zh_TW 未加载时，退回同语言族的 zh_CN
    const std::string language = LanguagePart(locale);
    for (const auto& pair : table_) {
        if (pair.first == locale || LanguagePart(pair.first) != language) {
            continue;
        }
        const auto found = pair.second.find(std::string(key));
        if (found != pair.second.end()) {
            return &found->second;
        }
    }
    return nullptr;
}

bool Translator::SetLocale(std::string_view locale) {
    std::lock_guard<std::mutex> lock(mutex_);
    locale_ = locale.empty() ? options_.fallbackLocale : std::string(locale);
    if (table_.find(locale_) != table_.end()) {
        return true;
    }
    const std::string language = LanguagePart(locale_);
    for (const auto& pair : table_) {
        if (LanguagePart(pair.first) == language) {
            return true;
        }
    }
    return false;
}

std::string Translator::Locale() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return locale_;
}

void Translator::SetFallbackLocale(std::string_view locale) {
    if (locale.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    options_.fallbackLocale = std::string(locale);
}

std::string Translator::FallbackLocale() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return options_.fallbackLocale;
}

std::vector<std::string> Translator::Locales() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> locales;
    locales.reserve(table_.size());
    for (const auto& pair : table_) {
        locales.push_back(pair.first);
    }
    return locales;
}

bool Translator::Has(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return FindLocked(locale_, key) != nullptr || FindLocked(options_.fallbackLocale, key) != nullptr;
}

std::string Translator::Tr(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const std::string* value = FindLocked(locale_, key)) {
        return *value;
    }
    if (const std::string* value = FindLocked(options_.fallbackLocale, key)) {
        return *value;
    }
    missing_.insert(std::string(key));
    return options_.returnKeyWhenMissing ? std::string(key) : std::string();
}

std::vector<std::string> Translator::MissingKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(missing_.begin(), missing_.end());
}

void Translator::ClearMissingKeys() {
    std::lock_guard<std::mutex> lock(mutex_);
    missing_.clear();
}

void Translator::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    table_.clear();
    missing_.clear();
    locale_ = options_.fallbackLocale;
}

Translator& Translator::Default() {
    static Translator instance;
    return instance;
}

bool SetLocale(std::string_view locale) { return Translator::Default().SetLocale(locale); }

std::string CurrentLocale() { return Translator::Default().Locale(); }

std::string Tr(std::string_view key) { return Translator::Default().Tr(key); }

}  // namespace i18n
