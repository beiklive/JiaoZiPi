#pragma once

// 着色器预设解析（独立模块，不依赖宿主项目、不依赖图形 API）
//
// 兼容 RetroArch 的 key = value 预设：.slangp / .glslp / .cgp
//   shaders = 2
//   shader0 = "shaders/scanline.slang"
//   filter_linear0 = false
//   scale_type0 = viewport
//   scale0 = 2.0
//   textures = "LUT"
//   LUT = "textures/lut.png"
//   LUT_linear = true
//   parameters = "SCANLINE_OPACITY"
//   SCANLINE_OPACITY = 0.35
//   #reference "base.slangp"          <- 继承：被引用预设先合并，本文件的值覆盖它
//
// 只做「读」：不编译、不创建图形资源、不碰文件系统之外的东西。
// 参数定义（#pragma parameter）从着色器源码收集，见 ShaderSource.h。
//
// 迁移到其他项目：复制 shader/ 目录即可；命名空间若与宿主冲突，全局替换 `namespace shader`。

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace shader {

enum class Format { Unknown, Slang, Glsl, Cg };

enum class ScaleType { Unspecified, Source, Viewport, Absolute };

enum class WrapMode { Unspecified, ClampToBorder, ClampToEdge, Repeat, MirroredRepeat };

enum class FilterMode { Unspecified, Nearest, Linear };

struct Pass {
    std::string source;   // shader<i>，已按预设目录解析
    std::string alias;    // alias<i>

    FilterMode filter = FilterMode::Unspecified;    // filter_linear<i>
    WrapMode wrap = WrapMode::Unspecified;          // wrap_mode<i>
    std::size_t frameCountMod = 0;                  // frame_count_mod<i>
    bool mipmapInput = false;                       // mipmap_input<i>
    bool srgbFramebuffer = false;                   // srgb_framebuffer<i>
    bool floatFramebuffer = false;                  // float_framebuffer<i>
    bool rgb10Framebuffer = false;                  // rgb10_framebuffer<i>

    ScaleType scaleType = ScaleType::Unspecified;   // scale_type<i>（同时作用于两轴）
    float scale = 1.0f;                             // scale<i>
    ScaleType scaleTypeX = ScaleType::Unspecified;  // scale_type_x<i>（覆盖单轴）
    float scaleX = 1.0f;                            // scale_x<i>
    ScaleType scaleTypeY = ScaleType::Unspecified;  // scale_type_y<i>
    float scaleY = 1.0f;                            // scale_y<i>

    ScaleType effectiveScaleTypeX() const;
    ScaleType effectiveScaleTypeY() const;
};

struct Texture {
    std::string id;     // textures 列表里的名字
    std::string path;   // <id>
    FilterMode filter = FilterMode::Unspecified;  // <id>_linear
    WrapMode wrap = WrapMode::Unspecified;        // <id>_wrap_mode
    bool mipmap = false;                          // <id>_mipmap
};

struct Parameter {
    std::string name;
    std::string description;
    float initial = 0.0f;   // #pragma parameter 的初始值
    float minimum = 0.0f;
    float maximum = 0.0f;
    float step = 0.0f;
    bool hasStep = false;
    float value = 0.0f;     // 实际使用值（预设覆盖，默认 = initial）
    bool fromPreset = false;
};

class Preset {
public:
    struct Options {
        unsigned maxReferenceDepth = 16;
        // 是否在 Load() 里读取着色器源码收集 #pragma parameter
        bool resolveParameters = true;
        // #include 搜索目录（着色器源码内）
        std::vector<std::string> includeDirectories;
        // 路径通配符替换，如 {"$CORE$", "mgba"}
        std::map<std::string, std::string> wildcards;
    };

    Preset();
    explicit Preset(Options options);

    // 读取预设文件（含 #reference 链）
    bool Load(std::string_view path);
    // 从内存文本解析，directory 用于解析相对路径
    bool LoadFromString(std::string_view text, std::string_view directory,
                        std::string_view sourceName = "<memory>");
    // 读取每个 pass 的着色器源码，收集 #pragma parameter 并按预设值覆盖
    bool ResolveParameters();

    Format format() const { return format_; }
    const std::string& path() const { return path_; }
    const std::string& directory() const { return directory_; }
    const std::vector<Pass>& passes() const { return passes_; }
    const std::vector<Texture>& textures() const { return textures_; }
    const std::vector<Parameter>& parameters() const { return parameters_; }
    // 引用链：从最底层被引用的预设到本文件
    const std::vector<std::string>& references() const { return references_; }

    const Parameter* FindParameter(std::string_view name) const;
    bool SetParameterValue(std::string_view name, float value);
    // 展开通配符 + 相对路径解析
    std::string ResolvePath(std::string_view relative) const;

    static Format FormatFromExtension(std::string_view path);

    const std::string& LastError() const { return lastError_; }
    const std::vector<std::string>& Warnings() const { return warnings_; }
    void ClearWarnings();

private:
    bool Apply(const std::map<std::string, std::string>& values);

    Options options_;
    Format format_ = Format::Unknown;
    std::string path_;
    std::string directory_;
    std::vector<Pass> passes_;
    std::vector<Texture> textures_;
    std::vector<Parameter> parameters_;
    std::vector<std::string> references_;
    std::string lastError_;
    std::vector<std::string> warnings_;
};

}  // namespace shader
