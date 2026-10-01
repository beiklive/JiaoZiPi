#include "ShaderSource.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace shader {
namespace {

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

std::string Normalize(std::string_view path) {
    std::string collapsed;
    collapsed.reserve(path.size());
    for (const char raw : path) {
        const char ch = raw == '\\' ? '/' : raw;
        if (ch == '/' && !collapsed.empty() && collapsed.back() == '/') {
            continue;
        }
        collapsed.push_back(ch);
    }
    while (collapsed.size() > 1 && collapsed.back() == '/') {
        collapsed.pop_back();
    }
    return collapsed;
}

bool IsAbsolutePath(std::string_view path) {
    if (path.empty()) {
        return false;
    }
    if (path.front() == '/') {
        return true;
    }
    const std::size_t colon = path.find(':');
    return colon != std::string_view::npos && colon > 0 && colon + 1 < path.size() &&
           path[colon + 1] == '/';
}

std::string DirectoryName(std::string_view path) {
    const std::string normalized = Normalize(path);
    const std::size_t slash = normalized.find_last_of('/');
    return slash == std::string::npos ? std::string() : normalized.substr(0, slash);
}

std::string JoinPath(std::string_view base, std::string_view child) {
    if (child.empty()) {
        return Normalize(base);
    }
    if (base.empty() || IsAbsolutePath(child)) {
        return Normalize(child);
    }
    std::string out = Normalize(base);
    if (!out.empty() && out.back() != '/' && out.back() != ':') {
        out.push_back('/');
    }
    out.append(child.data(), child.size());
    return Normalize(out);
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
    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF &&
        static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    return true;
}

std::vector<std::string> SplitArguments(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t position = 0;
    while (position < text.size()) {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) {
            ++position;
        }
        if (position >= text.size()) {
            break;
        }
        std::string token;
        if (text[position] == '"') {
            ++position;
            while (position < text.size() && text[position] != '"') {
                token.push_back(text[position]);
                ++position;
            }
            if (position < text.size()) {
                ++position;  // 跳过闭引号
            }
        } else {
            while (position < text.size() &&
                   std::isspace(static_cast<unsigned char>(text[position])) == 0) {
                token.push_back(text[position]);
                ++position;
            }
        }
        tokens.push_back(std::move(token));
    }
    return tokens;
}

// #pragma parameter NAME "说明" 初始值 最小值 最大值 [步长]
bool ParseParameterPragma(std::string_view line, ParameterSpec& out) {
    constexpr std::string_view kPragma = "#pragma parameter";
    if (line.compare(0, kPragma.size(), kPragma) != 0) {
        return false;
    }
    const std::vector<std::string> tokens = SplitArguments(Trim(line.substr(kPragma.size())));
    if (tokens.size() < 5) {
        return false;
    }
    out = ParameterSpec();
    out.name = tokens[0];
    out.description = tokens[1];
    out.initial = std::strtof(tokens[2].c_str(), nullptr);
    out.minimum = std::strtof(tokens[3].c_str(), nullptr);
    out.maximum = std::strtof(tokens[4].c_str(), nullptr);
    if (tokens.size() >= 6) {
        out.step = std::strtof(tokens[5].c_str(), nullptr);
        out.hasStep = true;
    }
    return !out.name.empty();
}

bool IsIgnoredPragma(std::string_view line) {
    constexpr std::string_view kPrefixes[] = {"#pragma parameter", "#pragma stage", "#pragma name",
                                             "#pragma format"};
    for (const std::string_view prefix : kPrefixes) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

// 解析 `#include "x"` / `#include <x>`，返回被包含路径
bool ParseInclude(std::string_view line, std::string& out) {
    constexpr std::string_view kInclude = "#include";
    if (line.compare(0, kInclude.size(), kInclude) != 0) {
        return false;
    }
    const std::string_view rest = Trim(line.substr(kInclude.size()));
    if (rest.size() < 3) {
        return false;
    }
    const char open = rest.front();
    const char close = open == '"' ? '"' : (open == '<' ? '>' : '\0');
    if (close == '\0') {
        return false;
    }
    const std::size_t end = rest.find(close, 1);
    if (end == std::string_view::npos) {
        return false;
    }
    out = std::string(rest.substr(1, end - 1));
    return true;
}

struct ExpandContext {
    const PreprocessOptions* options = nullptr;
    std::vector<std::string> stack;      // 当前包含链（循环保护）
    std::vector<ParameterSpec> parameters;
    std::vector<std::string> warnings;
    int depth = 0;
};

bool AddParameter(ExpandContext& context, const ParameterSpec& parameter, const std::string& source) {
    for (const ParameterSpec& existing : context.parameters) {
        if (existing.name == parameter.name) {
            context.warnings.push_back("参数重复声明，已忽略后者：" + parameter.name + "（" +
                                       source + "）");
            return false;
        }
    }
    context.parameters.push_back(parameter);
    return true;
}

std::string ResolveInclude(const std::string& name, std::string_view currentDirectory,
                           const PreprocessOptions& options) {
    if (IsAbsolutePath(name)) {
        return Normalize(name);
    }
    std::string candidate = JoinPath(currentDirectory, name);
    std::string probe;
    if (ReadFile(candidate, probe)) {
        return candidate;
    }
    for (const std::string& directory : options.includeDirectories) {
        candidate = JoinPath(directory, name);
        if (ReadFile(candidate, probe)) {
            return candidate;
        }
    }
    return std::string();
}

bool Expand(std::string_view text, std::string_view directory, ExpandContext& context,
            std::string& out) {
    if (context.depth > context.options->maxIncludeDepth) {
        context.warnings.push_back("include 层级超过上限，停止展开：" + std::string(directory));
        return false;
    }

    std::size_t position = 0;
    while (position <= text.size()) {
        const std::size_t newline = text.find('\n', position);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        std::string_view line(text.data() + position, end - position);
        position = end + 1;

        std::string_view trimmed = line;
        if (!trimmed.empty() && trimmed.back() == '\r') {
            trimmed.remove_suffix(1);
        }
        const std::string_view stripped = Trim(trimmed);

        ParameterSpec parameter;
        if (ParseParameterPragma(stripped, parameter)) {
            AddParameter(context, parameter, std::string(directory));
            if (!context.options->stripPragmas) {
                out.append(line.data(), line.size());
                out.push_back('\n');
            }
        } else if (context.options->stripPragmas && IsIgnoredPragma(stripped)) {
            // 剔除
        } else {
            std::string includeName;
            if (ParseInclude(stripped, includeName)) {
                const std::string resolved = ResolveInclude(includeName, directory, *context.options);
                if (resolved.empty()) {
                    context.warnings.push_back("找不到 include 文件：" + includeName);
                    out.append(line.data(), line.size());
                    out.push_back('\n');
                } else if (std::find(context.stack.begin(), context.stack.end(), resolved) !=
                           context.stack.end()) {
                    context.warnings.push_back("循环 include，已跳过：" + resolved);
                } else {
                    std::string included;
                    if (!ReadFile(resolved, included)) {
                        context.warnings.push_back("无法读取 include 文件：" + resolved);
                    } else {
                        context.stack.push_back(resolved);
                        ++context.depth;
                        Expand(included, DirectoryName(resolved), context, out);
                        --context.depth;
                        context.stack.pop_back();
                    }
                }
            } else {
                out.append(line.data(), line.size());
                out.push_back('\n');
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
    }
    return true;
}

// 首个有效行若是 #version，返回该行结束（含换行）的位置；否则返回 0。
// GLSL 要求 #version 必须是第一条有效语句，所以注入的 #define 只能放在它后面。
std::size_t VersionLineEnd(std::string_view text) {
    std::size_t position = 0;
    while (position < text.size()) {
        const std::size_t newline = text.find('\n', position);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        const std::string_view line = Trim(text.substr(position, end - position));
        if (!line.empty()) {
            if (line.compare(0, 8, "#version") == 0) {
                return newline == std::string_view::npos ? text.size() : newline + 1;
            }
            if (line.front() != '/' || (line.size() > 1 && line[1] != '/' && line[1] != '*')) {
                return 0;
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        position = newline + 1;
    }
    return 0;
}

std::string FormatFloat(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6g", static_cast<double>(value));
    return buffer;
}

}  // namespace

Stage StageFromPath(std::string_view path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos) {
        return Stage::Unknown;
    }
    std::string extension(path.substr(dot + 1));
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == "vert" || extension == "vs") {
        return Stage::Vertex;
    }
    if (extension == "frag" || extension == "fs" || extension == "slang") {
        return Stage::Fragment;
    }
    if (extension == "comp" || extension == "cs") {
        return Stage::Compute;
    }
    return Stage::Unknown;
}

const char* StageDefine(Stage stage) {
    switch (stage) {
        case Stage::Vertex: return "VERTEX";
        case Stage::Fragment: return "FRAGMENT";
        case Stage::Compute: return "COMPUTE";
        case Stage::Unknown:
        default: return nullptr;
    }
}

bool CollectParameters(std::string_view source, std::vector<ParameterSpec>& out,
                       std::vector<std::string>& warnings) {
    std::size_t position = 0;
    while (position <= source.size()) {
        const std::size_t newline = source.find('\n', position);
        const std::size_t end = newline == std::string_view::npos ? source.size() : newline;
        const std::string_view line = Trim(source.substr(position, end - position));
        ParameterSpec parameter;
        if (ParseParameterPragma(line, parameter)) {
            bool duplicate = false;
            for (const ParameterSpec& existing : out) {
                if (existing.name == parameter.name) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                warnings.push_back("参数重复声明，已忽略后者：" + parameter.name);
            } else {
                out.push_back(parameter);
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        position = newline + 1;
    }
    return true;
}

PreprocessResult PreprocessSource(std::string_view source, std::string_view directory,
                                  const PreprocessOptions& options) {
    PreprocessResult result;

    ExpandContext context;
    context.options = &options;

    std::string body;
    Expand(source, directory, context, body);

    result.parameters = context.parameters;
    result.warnings = context.warnings;

    std::string inject;
    if (options.defineStage) {
        if (const char* define = StageDefine(options.stage)) {
            inject += std::string("#define ") + define + " 1\n";
        }
    }
    if (options.defineParameterUniform && !result.parameters.empty()) {
        inject += "#define PARAMETER_UNIFORM 1\n";
    }
    for (const std::string& define : options.defines) {
        inject += "#define " + define + "\n";
    }
    for (const auto& parameter : options.parameterValues) {
        inject += "#define " + parameter.first + " " + FormatFloat(parameter.second) + "\n";
    }
    if (!inject.empty()) {
        inject += "\n";
    }

    const std::size_t versionEnd = VersionLineEnd(body);
    if (versionEnd > 0) {
        // 源码自带 #version：注解放到它之后，否则 glslang/GL 会报 "#version must occur first"
        result.source.assign(body.data(), versionEnd);
        result.source += inject;
        result.source.append(body.data() + versionEnd, body.size() - versionEnd);
    } else {
        if (!options.version.empty()) {
            result.source += "#version " + options.version + "\n";
        }
        result.source += inject;
        result.source += body;
    }
    result.ok = true;
    return result;
}

PreprocessResult PreprocessFile(std::string_view path, const PreprocessOptions& options) {
    PreprocessResult result;
    std::string text;
    if (!ReadFile(std::string(path), text)) {
        result.ok = false;
        result.warnings.push_back("无法读取着色器源码：" + std::string(path));
        return result;
    }
    PreprocessOptions effective = options;
    if (effective.stage == Stage::Unknown) {
        effective.stage = StageFromPath(path);
    }
    result = PreprocessSource(text, DirectoryName(path), effective);
    return result;
}

}  // namespace shader
