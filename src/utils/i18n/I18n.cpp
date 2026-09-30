#include "I18n.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace i18n {
namespace {

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

std::string Unescape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out.push_back(value[i]);
            continue;
        }
        switch (value[++i]) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '\\': out.push_back('\\'); break;
            default: out.push_back('\\'); out.push_back(value[i]); break;
        }
    }
    return out;
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
    if (name.size() <= extension.size()) {
        return false;
    }
    return name.compare(name.size() - extension.size(), extension.size(), extension) == 0;
}

std::string StripExtension(std::string_view name, std::string_view extension) {
    return std::string(name.substr(0, name.size() - extension.size()));
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
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);  // 去掉 UTF-8 BOM
    }

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
        line = Trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            continue;
        }
        const std::string_view key = Trim(line.substr(0, equals));
        if (key.empty()) {
            continue;
        }
        target[std::string(key)] = Unescape(Trim(line.substr(equals + 1)));

        if (newline == std::string::npos) {
            break;
        }
    }
    return true;
}

std::size_t Translator::LoadDirectory(std::string_view directory, std::string_view extension) {
    const std::string dir(directory);
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
        if (!HasExtension(name, extension)) {
            continue;
        }
        if (LoadFile(StripExtension(name, extension), JoinPath(dir, name))) {
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
        if (!HasExtension(name, extension)) {
            continue;
        }
        if (LoadFile(StripExtension(name, extension), JoinPath(dir, name))) {
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
