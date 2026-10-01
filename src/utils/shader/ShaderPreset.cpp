#include "ShaderPreset.h"

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

std::vector<std::string> SplitList(std::string_view value) {
    std::vector<std::string> out;
    std::size_t position = 0;
    while (position <= value.size()) {
        std::size_t separator = value.find(';', position);
        if (separator == std::string_view::npos) {
            separator = value.size();
        }
        const std::string_view item = Trim(value.substr(position, separator - position));
        if (!item.empty()) {
            out.emplace_back(item);
        }
        if (separator == value.size()) {
            break;
        }
        position = separator + 1;
    }
    return out;
}

WrapMode WrapFromString(std::string_view text) {
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "clamp_to_border") {
        return WrapMode::ClampToBorder;
    }
    if (lowered == "clamp_to_edge") {
        return WrapMode::ClampToEdge;
    }
    if (lowered == "repeat") {
        return WrapMode::Repeat;
    }
    if (lowered == "mirrored_repeat") {
        return WrapMode::MirroredRepeat;
    }
    return WrapMode::Unspecified;
}

ScaleType ScaleFromString(std::string_view text) {
    if (text == "source") {
        return ScaleType::Source;
    }
    if (text == "viewport") {
        return ScaleType::Viewport;
    }
    if (text == "absolute") {
        return ScaleType::Absolute;
    }
    return ScaleType::Unspecified;
}

std::string ExpandWildcards(std::string text, const std::map<std::string, std::string>& wildcards) {
    for (const auto& wildcard : wildcards) {
        std::size_t position = 0;
        while ((position = text.find(wildcard.first, position)) != std::string::npos) {
            text.replace(position, wildcard.first.size(), wildcard.second);
            position += wildcard.second.size();
        }
    }
    return text;
}

using ValueMap = std::map<std::string, std::string>;

struct RawPreset {
    std::vector<std::pair<std::string, std::string>> entries;
    std::vector<std::string> references;
};

void ParseConfigText(std::string_view text, RawPreset& out, std::vector<std::string>& warnings) {
    std::size_t position = 0;
    while (position <= text.size()) {
        const std::size_t newline = text.find('\n', position);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        std::string_view line(text.data() + position, end - position);
        position = end + 1;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        line = Trim(line);

        if (!line.empty() && line.front() == '#') {
            constexpr std::string_view kReference = "#reference";
            if (line.compare(0, kReference.size(), kReference) == 0) {
                const std::string reference = Unquote(Trim(line.substr(kReference.size())));
                if (reference.empty()) {
                    warnings.push_back("空的 #reference 指令已忽略");
                } else {
                    out.references.push_back(reference);
                }
            }
        } else if (!line.empty()) {
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos) {
                warnings.push_back("跳过无法解析的行：" + std::string(line));
            } else {
                const std::string key(Trim(line.substr(0, equals)));
                const std::string value = Unquote(Trim(line.substr(equals + 1)));
                if (!key.empty()) {
                    out.entries.emplace_back(key, value);
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
    }
}

// 解析 #reference 链：被引用的预设先合并，本文件的值覆盖之
class ChainLoader {
public:
    ChainLoader(const Preset::Options& options, std::vector<std::string>& warnings,
                std::vector<std::string>& chain)
        : options_(options), warnings_(warnings), chain_(chain) {}

    bool LoadFile(const std::string& path, unsigned depth) {
        if (depth > options_.maxReferenceDepth) {
            warnings_.push_back("引用层级超过上限 " + std::to_string(options_.maxReferenceDepth) +
                                "，已停止：" + path);
            return false;
        }
        if (std::find(visited_.begin(), visited_.end(), path) != visited_.end()) {
            warnings_.push_back("检测到循环引用，已跳过：" + path);
            return false;
        }
        std::string text;
        if (!ReadFile(path, text)) {
            warnings_.push_back("无法读取预设：" + path);
            return false;
        }
        visited_.push_back(path);

        RawPreset raw;
        ParseConfigText(text, raw, warnings_);

        const std::string directory = DirectoryName(path);
        for (const std::string& reference : raw.references) {
            std::string resolved = ExpandWildcards(reference, options_.wildcards);
            if (!IsAbsolutePath(resolved)) {
                resolved = JoinPath(directory, resolved);
            }
            LoadFile(resolved, depth + 1);
        }

        for (const auto& entry : raw.entries) {
            values_[entry.first] = entry.second;
        }
        chain_.push_back(path);
        return true;
    }

    const ValueMap& values() const { return values_; }

private:
    const Preset::Options& options_;
    std::vector<std::string>& warnings_;
    std::vector<std::string>& chain_;
    std::vector<std::string> visited_;
    ValueMap values_;
};

}  // namespace

ScaleType Pass::effectiveScaleTypeX() const {
    return scaleTypeX != ScaleType::Unspecified ? scaleTypeX : scaleType;
}

ScaleType Pass::effectiveScaleTypeY() const {
    return scaleTypeY != ScaleType::Unspecified ? scaleTypeY : scaleType;
}

Preset::Preset() : Preset(Options()) {}

Preset::Preset(Options options) : options_(std::move(options)) {}

Format Preset::FormatFromExtension(std::string_view path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos) {
        return Format::Unknown;
    }
    std::string extension(path.substr(dot + 1));
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == "slangp") {
        return Format::Slang;
    }
    if (extension == "glslp") {
        return Format::Glsl;
    }
    if (extension == "cgp") {
        return Format::Cg;
    }
    return Format::Unknown;
}

std::string Preset::ResolvePath(std::string_view relative) const {
    std::string resolved = ExpandWildcards(std::string(relative), options_.wildcards);
    if (IsAbsolutePath(resolved)) {
        return Normalize(resolved);
    }
    return JoinPath(directory_, resolved);
}

bool Preset::Load(std::string_view path) {
    path_ = Normalize(path);
    directory_ = DirectoryName(path_);
    format_ = FormatFromExtension(path_);
    passes_.clear();
    textures_.clear();
    parameters_.clear();
    references_.clear();
    lastError_.clear();
    warnings_.clear();

    ChainLoader loader(options_, warnings_, references_);
    if (!loader.LoadFile(path_, 0) || !Apply(loader.values())) {
        if (lastError_.empty()) {
            lastError_ = "预设加载失败：" + path_;
        }
        return false;
    }
    if (options_.resolveParameters) {
        ResolveParameters();
    }
    return true;
}

bool Preset::LoadFromString(std::string_view text, std::string_view directory,
                            std::string_view sourceName) {
    path_ = std::string(sourceName);
    directory_ = Normalize(directory);
    format_ = FormatFromExtension(sourceName);
    passes_.clear();
    textures_.clear();
    parameters_.clear();
    references_.clear();
    lastError_.clear();
    warnings_.clear();

    RawPreset raw;
    ParseConfigText(text, raw, warnings_);

    ValueMap values;
    if (!raw.references.empty()) {
        ChainLoader loader(options_, warnings_, references_);
        for (const std::string& reference : raw.references) {
            std::string resolved = ExpandWildcards(reference, options_.wildcards);
            if (!IsAbsolutePath(resolved)) {
                resolved = JoinPath(directory_, resolved);
            }
            loader.LoadFile(resolved, 1);
        }
        values = loader.values();
    }
    for (const auto& entry : raw.entries) {
        values[entry.first] = entry.second;
    }

    if (!Apply(values)) {
        return false;
    }
    if (options_.resolveParameters) {
        ResolveParameters();
    }
    return true;
}

bool Preset::Apply(const ValueMap& values) {
    const auto getString = [&values](const std::string& key, std::string& out) {
        const auto found = values.find(key);
        if (found == values.end()) {
            return false;
        }
        out = found->second;
        return true;
    };
    const auto getFloat = [&values](const std::string& key, float& out) {
        const auto found = values.find(key);
        if (found == values.end() || found->second.empty()) {
            return false;
        }
        out = std::strtof(found->second.c_str(), nullptr);
        return true;
    };
    const auto getBool = [&values](const std::string& key, bool& out) {
        const auto found = values.find(key);
        if (found == values.end()) {
            return false;
        }
        const std::string& raw = found->second;
        out = raw == "true" || raw == "1" || raw == "TRUE" || raw == "True";
        return true;
    };
    const auto getInt = [&values](const std::string& key, int& out) {
        const auto found = values.find(key);
        if (found == values.end() || found->second.empty()) {
            return false;
        }
        out = static_cast<int>(std::strtol(found->second.c_str(), nullptr, 10));
        return true;
    };

    if (values.find("shader0") == values.end()) {
        lastError_ = "预设里没有 shader0，不是有效的着色器链";
        return false;
    }

    int shaderCount = 0;
    if (!getInt("shaders", shaderCount) || shaderCount <= 0) {
        shaderCount = 1;
        std::string probe;
        while (getString("shader" + std::to_string(shaderCount), probe)) {
            ++shaderCount;
        }
        warnings_.push_back("缺少 shaders 键，按实际 pass 数读取：" + std::to_string(shaderCount));
    }

    for (int i = 0; i < shaderCount; ++i) {
        const std::string suffix = std::to_string(i);
        Pass pass;
        if (!getString("shader" + suffix, pass.source)) {
            warnings_.push_back("shader" + suffix + " 缺失，忽略该 pass 及其后续");
            break;
        }
        pass.source = ResolvePath(pass.source);
        getString("alias" + suffix, pass.alias);

        bool flag = false;
        std::string text;
        int number = 0;
        if (getBool("filter_linear" + suffix, flag)) {
            pass.filter = flag ? FilterMode::Linear : FilterMode::Nearest;
        }
        if (getString("wrap_mode" + suffix, text)) {
            pass.wrap = WrapFromString(text);
        }
        if (getInt("frame_count_mod" + suffix, number)) {
            pass.frameCountMod = number > 0 ? static_cast<std::size_t>(number) : 0;
        }
        if (getBool("mipmap_input" + suffix, flag)) {
            pass.mipmapInput = flag;
        }
        if (getBool("srgb_framebuffer" + suffix, flag)) {
            pass.srgbFramebuffer = flag;
        }
        if (getBool("float_framebuffer" + suffix, flag)) {
            pass.floatFramebuffer = flag;
        }
        if (getBool("rgb10_framebuffer" + suffix, flag)) {
            pass.rgb10Framebuffer = flag;
        }
        if (getString("scale_type" + suffix, text)) {
            pass.scaleType = ScaleFromString(text);
        }
        getFloat("scale" + suffix, pass.scale);
        if (getString("scale_type_x" + suffix, text)) {
            pass.scaleTypeX = ScaleFromString(text);
        }
        getFloat("scale_x" + suffix, pass.scaleX);
        if (getString("scale_type_y" + suffix, text)) {
            pass.scaleTypeY = ScaleFromString(text);
        }
        getFloat("scale_y" + suffix, pass.scaleY);
        passes_.push_back(std::move(pass));
    }

    std::string textureList;
    if (getString("textures", textureList)) {
        for (const std::string& id : SplitList(textureList)) {
            Texture texture;
            texture.id = id;
            if (!getString(id, texture.path)) {
                warnings_.push_back("textures 里的 " + id + " 没有对应路径，已跳过");
                continue;
            }
            texture.path = ResolvePath(texture.path);
            bool flag = false;
            std::string mode;
            if (getBool(id + "_linear", flag)) {
                texture.filter = flag ? FilterMode::Linear : FilterMode::Nearest;
            }
            if (getBool(id + "_mipmap", flag)) {
                texture.mipmap = flag;
            }
            if (getString(id + "_wrap_mode", mode)) {
                texture.wrap = WrapFromString(mode);
            }
            textures_.push_back(std::move(texture));
        }
    }

    std::string parameterList;
    if (getString("parameters", parameterList)) {
        for (const std::string& name : SplitList(parameterList)) {
            Parameter parameter;
            parameter.name = name;
            parameter.fromPreset = getFloat(name, parameter.value);
            parameters_.push_back(std::move(parameter));
        }
    }
    return true;
}

bool Preset::ResolveParameters() {
    const std::vector<Parameter> presetValues = parameters_;

    std::vector<ParameterSpec> specs;
    for (const Pass& pass : passes_) {
        if (pass.source.empty()) {
            continue;
        }
        // 走预处理：参数可能声明在被 #include 的文件里（RetroArch 也是这么做的）
        PreprocessOptions preprocess;
        preprocess.stage = Stage::Unknown;
        preprocess.defineStage = false;
        preprocess.defineParameterUniform = false;
        preprocess.stripPragmas = true;
        preprocess.includeDirectories = options_.includeDirectories;
        const PreprocessResult prepared = PreprocessFile(pass.source, preprocess);
        if (!prepared.ok) {
            warnings_.push_back("无法读取着色器源码（跳过参数收集）：" + pass.source);
            continue;
        }
        for (const std::string& warning : prepared.warnings) {
            warnings_.push_back(pass.source + "：" + warning);
        }
        for (const ParameterSpec& candidate : prepared.parameters) {
            const bool exists =
                std::any_of(specs.begin(), specs.end(), [&candidate](const ParameterSpec& item) {
                    return item.name == candidate.name;
                });
            if (exists) {
                warnings_.push_back("参数在多处声明，已忽略后者：" + candidate.name);
                continue;
            }
            specs.push_back(candidate);
        }
    }

    std::vector<Parameter> merged;
    merged.reserve(specs.size() + presetValues.size());
    for (const ParameterSpec& spec : specs) {
        Parameter parameter;
        parameter.name = spec.name;
        parameter.description = spec.description;
        parameter.initial = spec.initial;
        parameter.minimum = spec.minimum;
        parameter.maximum = spec.maximum;
        parameter.step = spec.step;
        parameter.hasStep = spec.hasStep;
        parameter.value = spec.initial;
        for (const Parameter& preset : presetValues) {
            if (preset.name == parameter.name && preset.fromPreset) {
                parameter.value = preset.value;
                parameter.fromPreset = true;
            }
        }
        merged.push_back(std::move(parameter));
    }
    for (const Parameter& preset : presetValues) {
        const bool declared =
            std::any_of(specs.begin(), specs.end(), [&preset](const ParameterSpec& item) {
                return item.name == preset.name;
            });
        if (declared) {
            continue;
        }
        warnings_.push_back("预设引用了源码未声明的参数，已保留：" + preset.name);
        merged.push_back(preset);
    }

    parameters_ = std::move(merged);
    return true;
}

const Parameter* Preset::FindParameter(std::string_view name) const {
    for (const Parameter& parameter : parameters_) {
        if (parameter.name == name) {
            return &parameter;
        }
    }
    return nullptr;
}

bool Preset::SetParameterValue(std::string_view name, float value) {
    for (Parameter& parameter : parameters_) {
        if (parameter.name == name) {
            parameter.value = value;
            parameter.fromPreset = true;
            return true;
        }
    }
    return false;
}

void Preset::ClearWarnings() { warnings_.clear(); }

}  // namespace shader
