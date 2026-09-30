#pragma once

// 极简字符串格式化：`{}` 顺序占位符，`{{` / `}}` 转义。
// 无第三方依赖、header-only、C++17，可单独复制到任意项目。
// 参数不足时保留 `{}` 原样输出，便于发现漏传。

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace strfmt {

namespace detail {

inline void ToText(std::string& out, bool value) { out += value ? "true" : "false"; }
inline void ToText(std::string& out, char value) { out.push_back(value); }
inline void ToText(std::string& out, const char* value) { out += value ? value : "(null)"; }
inline void ToText(std::string& out, const std::string& value) { out += value; }
inline void ToText(std::string& out, std::string_view value) { out.append(value.data(), value.size()); }

template <class T>
inline void ToText(std::string& out, const T& value) {
    std::ostringstream stream;
    stream << value;
    out += stream.str();
}

template <std::size_t Index, class Tuple>
void Expand(std::string& out, std::string_view format, const Tuple& args) {
    for (std::size_t i = 0; i < format.size(); ++i) {
        const char ch = format[i];
        if (ch == '{' && i + 1 < format.size()) {
            if (format[i + 1] == '{') {
                out.push_back('{');
                ++i;
                continue;
            }
            if (format[i + 1] == '}') {
                if constexpr (Index < std::tuple_size_v<Tuple>) {
                    ToText(out, std::get<Index>(args));
                    Expand<Index + 1>(out, format.substr(i + 2), args);
                    return;
                }
                out += "{}";
                ++i;
                continue;
            }
        } else if (ch == '}' && i + 1 < format.size() && format[i + 1] == '}') {
            out.push_back('}');
            ++i;
            continue;
        }
        out.push_back(ch);
    }
}

}  // namespace detail

template <class... Args>
std::string Format(std::string_view format, Args&&... args) {
    std::string out;
    out.reserve(format.size() + sizeof...(Args) * 16 + 16);
    detail::Expand<0>(out, format, std::forward_as_tuple(std::forward<Args>(args)...));
    return out;
}

}  // namespace strfmt
