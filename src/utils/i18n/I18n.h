#pragma once

// 通用多语言系统（独立模块，不依赖宿主项目）
//
// - 语言表来自 UTF-8 文本文件（`key = value`），也可运行时注册（内置语言表）
// - 查找链：当前语言 → 当前语言的语言前缀（zh_CN → zh）→ 回退语言 → key 本身
// - 支持 `{}` 占位符格式化，支持缺失 key 统计，便于查漏
// - 线程安全；C++17；仅依赖同仓库的 format/StrFormat.h（header-only）
//
// 语言文件格式（UTF-8）：
//   # 注释
//   app.title = 饺子皮
//   greeting = 欢迎使用 {0}      <- 占位符用 {} 顺序填充，不需要填序号
//
// 迁移到其他项目：复制 i18n/ 与 format/ 两个目录，把它们的上级目录加入头文件搜索路径，
// 并把 I18n.cpp 加进构建。该模块与 log/ 互不依赖，可单独使用。

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
        std::string fallbackLocale = "en_US";
        bool returnKeyWhenMissing = true;  // 缺失时返回 key 本身，便于定位
    };

    Translator();
    explicit Translator(Options options);

    // --- 语言表 -------------------------------------------------------------
    bool LoadFile(std::string_view locale, std::string_view path);
    // 扫描目录下所有后缀匹配的文件，文件名（去掉后缀）即语言名；返回加载数量
    std::size_t LoadDirectory(std::string_view directory, std::string_view extension = ".lang");

    void Register(std::string_view locale, std::initializer_list<Entry> entries);
    void Set(std::string_view locale, std::string_view key, std::string_view value);

    // --- 语言选择 -----------------------------------------------------------
    // 切到指定语言；返回 false 表示没有该语言的翻译，此时仍会切换（内容走回退链）
    bool SetLocale(std::string_view locale);
    std::string Locale() const;

    void SetFallbackLocale(std::string_view locale);
    std::string FallbackLocale() const;

    // 已加载/注册的语言列表（字典序）
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

    static Translator& Default();

private:
    const std::string* FindLocked(std::string_view locale, std::string_view key) const;
    static std::string LanguagePart(std::string_view locale);

    mutable std::mutex mutex_;
    Options options_;
    std::string locale_;
    std::map<std::string, std::map<std::string, std::string>> table_;
    mutable std::set<std::string> missing_;
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
