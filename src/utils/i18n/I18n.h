#pragma once

// 通用多语言系统（独立模块，不依赖宿主项目）
//
// - 语言表集中维护在单个 `language.json` 里，结构为 key → { 语言代号: 文本 }：
//     {
//       "app.name":  { "en": "JiaoZiPi", "zh": "饺子皮" },
//       "menu.quit": { "en": "Quit",     "zh": "退出" }
//     }
//   一个文件管所有语言，不按语言拆文件
// - 查找链：当前语言 → 同语言族（zh_TW → zh）→ 回退语言 → key 本身
// - 支持 `{}` 占位符格式化；缺失 key 会记录，便于查漏
// - 也可运行时注册 / 单条设置（把翻译编进二进制）
// - 线程安全；C++17
// - 依赖：format/StrFormat.h 与 third_party/nlohmann/json（都是 header-only）
//
// 迁移到其他项目：复制 i18n/ 与 format/ 两个目录，把它们的上级目录与 nlohmann/json 的
// include 目录加入头文件搜索路径，并把 I18n.cpp 加进构建。该模块与 log/ 互不依赖。

#include <cstddef>
#include <initializer_list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "format/StrFormat.h"

namespace i18n {

struct Entry {
    const char* key;
    const char* value;
};

class Translator {
public:
    struct Options {
        std::string fallbackLocale = "en";
        bool returnKeyWhenMissing = true;  // 缺失时返回 key 本身，便于定位
    };

    Translator();
    explicit Translator(Options options);

    // --- 语言表 -------------------------------------------------------------
    // 读取 language.json（key → { 语言代号: 文本 }）；失败返回 false，原因见 LastError()
    bool LoadFile(std::string_view path);
    // 从内存里的 JSON 文本加载，便于内置语言表与测试
    bool LoadString(std::string_view json, std::string_view source = "<memory>");

    void Register(std::string_view locale, std::initializer_list<Entry> entries);
    void Set(std::string_view locale, std::string_view key, std::string_view value);

    // --- 语言选择 -----------------------------------------------------------
    // 切到指定语言；返回 false 表示该语言没有任何条目（仍会切换，内容走回退链）
    bool SetLocale(std::string_view locale);
    std::string Locale() const;

    void SetFallbackLocale(std::string_view locale);
    std::string FallbackLocale() const;

    // 语言表中出现过的所有语言代号（字典序）
    std::vector<std::string> Locales() const;

    // --- 查询 ---------------------------------------------------------------
    bool Has(std::string_view key) const;
    std::string Tr(std::string_view key) const;

    template <class... Args>
    std::string Tr(std::string_view key, Args&&... args) const {
        return strfmt::Format(Tr(key), std::forward<Args>(args)...);
    }

    // 缺失 key 记录（去重）
    std::vector<std::string> MissingKeys() const;
    void ClearMissingKeys();

    void Clear();
    std::string LastError() const;

    static Translator& Default();

private:
    const std::string* FindLocked(std::string_view locale, std::string_view key) const;
    static std::string LanguagePart(std::string_view locale);

    mutable std::mutex mutex_;
    Options options_;
    std::string locale_;
    std::map<std::string, std::map<std::string, std::string>> table_;  // 语言 → key → 文本
    mutable std::set<std::string> missing_;
    std::string lastError_;
};

// 便捷接口：使用 Default()
bool SetLocale(std::string_view locale);
std::string CurrentLocale();
std::string Tr(std::string_view key);

template <class... Args>
std::string Tr(std::string_view key, Args&&... args) {
    return strfmt::Format(Translator::Default().Tr(key), std::forward<Args>(args)...);
}

}  // namespace i18n
