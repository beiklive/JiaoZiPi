#include "ChtFile.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>

namespace cht {
namespace {

constexpr std::string_view kCheatPrefix = "cheat";

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

void StripBom(std::string& text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

// 去掉一层引号并还原 \" \\；无引号则原样返回
std::string Unquote(std::string_view raw) {
    if (raw.size() < 2 || (raw.front() != '"' && raw.front() != '\'')) {
        return std::string(raw);
    }
    const char quote = raw.front();
    if (raw.back() != quote) {
        return std::string(raw);
    }
    std::string out;
    out.reserve(raw.size() - 2);
    for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
        const char ch = raw[i];
        if (ch == '\\' && i + 2 < raw.size() && (raw[i + 1] == quote || raw[i + 1] == '\\')) {
            out.push_back(raw[i + 1]);
            ++i;
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::string Quote(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char ch : value) {
        if (ch == '"' || ch == '\\') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

bool ParseBool(std::string_view value) {
    return value == "true" || value == "1" || value == "TRUE" || value == "True";
}

bool ParseUnsigned(std::string_view text, unsigned& out) {
    const std::string_view trimmed = Trim(text);
    if (trimmed.empty()) {
        return false;
    }
    unsigned value = 0;
    for (const char ch : trimmed) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10u + static_cast<unsigned>(ch - '0');
    }
    out = value;
    return true;
}

bool IsComment(std::string_view line) {
    return !line.empty() && (line.front() == '#' || line.front() == ';');
}

}  // namespace

File::File() : File(Options()) {}

File::File(Options options) : options_(options) {}

// ---------------------------------------------------------------------------
// 读取
// ---------------------------------------------------------------------------
bool File::Load(std::string_view path) {
    std::string text;
    if (!ReadFile(std::string(path), text)) {
        lastError_ = "无法读取金手指文件：" + std::string(path);
        return false;
    }
    return LoadFromString(text, path);
}

bool File::LoadFromString(std::string_view text, std::string_view source) {
    std::string owned(text);
    StripBom(owned);

    entries_.clear();
    fileExtras_.clear();
    lastError_.clear();
    warnings_.clear();

    std::map<unsigned, Entry> indexed;
    unsigned declared = 0;
    bool hasDeclared = false;
    unsigned parsedLines = 0;

    std::size_t position = 0;
    while (position <= owned.size()) {
        const std::size_t newline = owned.find('\n', position);
        const std::size_t end = newline == std::string::npos ? owned.size() : newline;
        std::string_view line(owned.data() + position, end - position);
        position = end + 1;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        line = Trim(line);
        if (!line.empty() && !IsComment(line)) {
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos) {
                warnings_.push_back("跳过无法解析的行：" + std::string(line));
            } else {
                const std::string key(Trim(line.substr(0, equals)));
                const std::string_view value = Trim(line.substr(equals + 1));
                ++parsedLines;

                if (key == "cheats") {
                    hasDeclared = ParseUnsigned(value, declared);
                } else if (key.compare(0, kCheatPrefix.size(), kCheatPrefix) == 0 &&
                           key.size() > kCheatPrefix.size()) {
                    std::size_t cursor = kCheatPrefix.size();
                    unsigned index = 0;
                    bool hasDigits = false;
                    while (cursor < key.size() && std::isdigit(static_cast<unsigned char>(key[cursor])) != 0) {
                        index = index * 10u + static_cast<unsigned>(key[cursor] - '0');
                        ++cursor;
                        hasDigits = true;
                    }
                    if (!hasDigits || cursor >= key.size()) {
                        warnings_.push_back("跳过没编号的键：" + key);
                    } else {
                        const std::string field = key.substr(cursor);  // 形如 "_desc"
                        Entry& entry = indexed[index];
                        if (field == "_desc") {
                            entry.description = Unquote(value);
                        } else if (field == "_code") {
                            entry.code = Unquote(value);
                        } else if (field == "_enable") {
                            entry.enabled = ParseBool(value);
                        } else if (field == "_big_endian") {
                            entry.bigEndian = ParseBool(value);
                        } else if (options_.preserveUnknownKeys) {
                            // 原样保存（含引号），保证写回不丢 RA 的元数据
                            entry.extra.emplace_back(field, std::string(value));
                        }
                    }
                } else if (options_.preserveUnknownKeys) {
                    fileExtras_.emplace_back(key, std::string(value));
                }
            }
        }

        if (newline == std::string::npos) {
            break;
        }
    }

    if (parsedLines == 0 && indexed.empty()) {
        warnings_.push_back("文件里没有金手指条目（或不是 .cht）：" + std::string(source));
    }

    constexpr unsigned kMaxCheats = 100000;
    if (hasDeclared && declared > kMaxCheats) {
        warnings_.push_back("cheats 声明为 " + std::to_string(declared) + "，超过上限，已忽略该声明");
        hasDeclared = false;
        declared = 0;
    }

    const unsigned highest = indexed.empty() ? 0u : indexed.rbegin()->first + 1u;
    if (hasDeclared && declared < highest) {
        warnings_.push_back("cheats 声明为 " + std::to_string(declared) + "，实际有 " +
                            std::to_string(highest) + " 条，已按实际条目数读取");
    } else if (hasDeclared && declared > highest) {
        warnings_.push_back("cheats 声明为 " + std::to_string(declared) + "，实际只定义了 " +
                            std::to_string(highest) + " 条，尾部已补空条目");
    }
    const unsigned count = std::max(hasDeclared ? declared : 0u, highest);

    entries_.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        const auto found = indexed.find(i);
        if (found == indexed.end()) {
            entries_.push_back(Entry());
            if (i < highest) {
                warnings_.push_back("cheat" + std::to_string(i) + " 缺失，已用空条目填充");
            }
            continue;
        }
        entries_.push_back(found->second);
    }
    return true;
}

// ---------------------------------------------------------------------------
// 写回
// ---------------------------------------------------------------------------
std::string File::ToString() const {
    std::string out;
    out += "cheats = " + std::to_string(entries_.size()) + "\n";

    for (const auto& extra : fileExtras_) {
        out += extra.first + " = " + extra.second + "\n";
    }

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const Entry& entry = entries_[i];
        const std::string prefix = "cheat" + std::to_string(i);
        out += "\n";
        out += prefix + "_desc = " +
               Quote(entry.description.empty() ? entry.code : entry.description) + "\n";
        out += prefix + "_code = " + Quote(entry.code) + "\n";
        out += prefix + "_enable = " + (entry.enabled ? "true" : "false") + "\n";
        out += prefix + "_big_endian = " + (entry.bigEndian ? "true" : "false") + "\n";
        for (const auto& extra : entry.extra) {
            out += prefix + extra.first + " = " + extra.second + "\n";
        }
    }
    return out;
}

bool File::Save(std::string_view path) const {
    const std::string text = ToString();
    std::FILE* file = std::fopen(std::string(path).c_str(), "wb");
    if (file == nullptr) {
        lastError_ = "无法写入金手指文件：" + std::string(path);
        return false;
    }
    const std::size_t written = std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    if (written != text.size()) {
        lastError_ = "写入不完整：" + std::string(path);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 读取
// ---------------------------------------------------------------------------
std::size_t File::Size() const { return entries_.size(); }

bool File::Empty() const { return entries_.empty(); }

const std::vector<Entry>& File::Entries() const { return entries_; }

std::vector<Entry>& File::Entries() { return entries_; }

const Entry* File::Get(std::size_t index) const {
    return index < entries_.size() ? &entries_[index] : nullptr;
}

Entry* File::Get(std::size_t index) {
    return index < entries_.size() ? &entries_[index] : nullptr;
}

std::size_t File::CountEnabled() const {
    return static_cast<std::size_t>(
        std::count_if(entries_.begin(), entries_.end(), [](const Entry& entry) { return entry.enabled; }));
}

int File::IndexOfCode(std::string_view code) const {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].code == code) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// 修改
// ---------------------------------------------------------------------------
std::size_t File::Add(Entry entry) {
    entries_.push_back(std::move(entry));
    return entries_.size() - 1;
}

std::size_t File::Add(std::string_view description, std::string_view code, bool enabled) {
    Entry entry;
    entry.description = std::string(description);
    entry.code = std::string(code);
    entry.enabled = enabled;
    return Add(std::move(entry));
}

bool File::Remove(std::size_t index) {
    if (index >= entries_.size()) {
        return false;
    }
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void File::Clear() { entries_.clear(); }

bool File::SetDescription(std::size_t index, std::string_view description) {
    Entry* entry = Get(index);
    if (entry == nullptr) {
        return false;
    }
    entry->description = std::string(description);
    return true;
}

bool File::SetCode(std::size_t index, std::string_view code) {
    Entry* entry = Get(index);
    if (entry == nullptr) {
        return false;
    }
    entry->code = std::string(code);
    return true;
}

bool File::SetEnabled(std::size_t index, bool enabled) {
    Entry* entry = Get(index);
    if (entry == nullptr) {
        return false;
    }
    entry->enabled = enabled;
    return true;
}

bool File::SetBigEndian(std::size_t index, bool bigEndian) {
    Entry* entry = Get(index);
    if (entry == nullptr) {
        return false;
    }
    entry->bigEndian = bigEndian;
    return true;
}

void File::EnableAll(bool enabled) {
    for (Entry& entry : entries_) {
        entry.enabled = enabled;
    }
}

// ---------------------------------------------------------------------------
// 诊断
// ---------------------------------------------------------------------------
std::string File::LastError() const { return lastError_; }

const std::vector<std::string>& File::Warnings() const { return warnings_; }

void File::ClearWarnings() { warnings_.clear(); }

}  // namespace cht
