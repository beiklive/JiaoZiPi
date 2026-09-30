#include "I18n.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <string>
#include <utility>

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

// 语言表条目值：接受字符串，数字/布尔转成文本，其它类型忽略
bool ValueToText(const nlohmann::json& value, std::string& out) {
    if (value.is_string()) {
        out = value.get<std::string>();
        return true;
    }
    if (value.is_primitive()) {
        out = value.dump();
        return true;
    }
    return false;
}

}  // namespace

Translator::Translator() : Translator(Options()) {}

Translator::Translator(Options options) : options_(std::move(options)) {
    if (options_.fallbackLocale.empty()) {
        options_.fallbackLocale = "en";
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

bool Translator::LoadFile(std::string_view path) {
    std::string content;
    if (!ReadFile(std::string(path), content)) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "无法读取语言表文件：" + std::string(path);
        return false;
    }
    return LoadString(content, path);
}

bool Translator::LoadString(std::string_view json, std::string_view source) {
    // allow_exceptions = false，解析失败时不抛异常；ignore_comments = true，允许 // 与 /* */ 注释
    const nlohmann::json document =
        nlohmann::json::parse(json.begin(), json.end(), nullptr, false, true);

    if (document.is_discarded()) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "语言表 JSON 解析失败：" + std::string(source);
        return false;
    }
    if (!document.is_object()) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "语言表根节点必须是对象：" + std::string(source);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto entry = document.begin(); entry != document.end(); ++entry) {
        if (!entry.value().is_object()) {
            continue;  // 结构不符：值必须是 { 语言代号: 文本 }
        }
        for (auto translation = entry.value().begin(); translation != entry.value().end();
             ++translation) {
            std::string text;
            if (ValueToText(translation.value(), text)) {
                table_[translation.key()][entry.key()] = std::move(text);
            }
        }
    }
    lastError_.clear();
    return true;
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

    // zh_TW 未提供时，退回同语言族的 zh
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
    lastError_.clear();
    locale_ = options_.fallbackLocale;
}

std::string Translator::LastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

Translator& Translator::Default() {
    static Translator instance;
    return instance;
}

bool SetLocale(std::string_view locale) { return Translator::Default().SetLocale(locale); }

std::string CurrentLocale() { return Translator::Default().Locale(); }

std::string Tr(std::string_view key) { return Translator::Default().Tr(key); }

}  // namespace i18n
