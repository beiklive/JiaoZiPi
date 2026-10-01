#pragma once

// 着色器编译（独立模块）
//
// 两条路径：
//   1) OpenGL / GLES：`PrepareSource()` 产出预处理后的 GLSL 源码，主机自己
//      glShaderSource + glCompileShader —— **零第三方依赖**
//   2) Vulkan：`CompileToSpirv()` 用 glslang 把 GLSL 编成 SPIR-V
//      —— 需要编译期定义 `JZP_SHADER_WITH_GLSLANG` 并链接 glslang
//
// 未定义该宏时 Vulkan 路径返回 ok = false 并在 log 里说明，OpenGL 路径照常可用，
// 因此模块本体不强制任何第三方依赖。

#include "ShaderSource.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shader {

// SPIR-V 魔数，用于校验输出
constexpr std::uint32_t kSpirvMagic = 0x07230203u;

struct CompileOptions {
    PreprocessOptions preprocess;      // 版本、stage、defines、参数、include 目录
    std::string entryPoint = "main";
    bool generateDebugInfo = false;    // 需要 SPIR-V 里带 OpSource 等调试信息
};

struct SpirvResult {
    bool ok = false;
    std::vector<std::uint32_t> words;
    std::string log;
};

class Compiler {
public:
    // 本次编译是否带上了 glslang（Vulkan/SPIR-V 路径）
    static bool SpirvBackendAvailable();
    static const char* SpirvBackendName();

    // --- OpenGL / GLES 路径：产出可直接编译的源码 ---
    static PreprocessResult PrepareSource(std::string_view path, const CompileOptions& options);
    static PreprocessResult PrepareSourceText(std::string_view source, std::string_view directory,
                                              const CompileOptions& options);

    // --- Vulkan 路径：GLSL → SPIR-V ---
    static SpirvResult CompileToSpirv(std::string_view path, const CompileOptions& options);
    static SpirvResult CompileTextToSpirv(std::string_view source, std::string_view directory,
                                          const CompileOptions& options);
};

}  // namespace shader
