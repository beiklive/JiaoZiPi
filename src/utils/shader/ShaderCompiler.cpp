#include "ShaderCompiler.h"

#include <mutex>

#if defined(JZP_SHADER_WITH_GLSLANG)
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

// SPIRV 头的路径与引入方式有关：源码树里是 <SPIRV/...>，安装后是 <glslang/SPIRV/...>
#if defined(__has_include)
#if __has_include(<glslang/SPIRV/GlslangToSpv.h>)
#include <glslang/SPIRV/GlslangToSpv.h>
#else
#include <SPIRV/GlslangToSpv.h>
#endif
#else
#include <SPIRV/GlslangToSpv.h>
#endif
#endif

namespace shader {
namespace {

CompileOptions WithStageFromPath(const CompileOptions& options, std::string_view path) {
    CompileOptions effective = options;
    if (effective.preprocess.stage == Stage::Unknown) {
        effective.preprocess.stage = StageFromPath(path);
    }
    return effective;
}

#if defined(JZP_SHADER_WITH_GLSLANG)
std::once_flag g_initOnce;

void EnsureInitialized() {
    std::call_once(g_initOnce, [] { glslang::InitializeProcess(); });
}

EShLanguage ToGlslangStage(Stage stage) {
    switch (stage) {
        case Stage::Vertex: return EShLangVertex;
        case Stage::Fragment: return EShLangFragment;
        case Stage::Compute: return EShLangCompute;
        case Stage::Unknown:
        default: return EShLangCount;
    }
}

bool CompileWithGlslang(const std::string& source, Stage stage, const CompileOptions& options,
                        SpirvResult& result) {
    EnsureInitialized();

    const EShLanguage language = ToGlslangStage(stage);
    if (language == EShLangCount) {
        result.log = "无法确定着色器阶段（Vertex / Fragment / Compute）";
        return false;
    }

    glslang::TShader shader(language);
    const char* text = source.c_str();
    shader.setStrings(&text, 1);
    shader.setEntryPoint(options.entryPoint.c_str());
    shader.setSourceEntryPoint(options.entryPoint.c_str());
    shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

    constexpr int kDefaultVersion = 450;
    if (!shader.parse(GetDefaultResources(), kDefaultVersion, false, EShMsgDefault)) {
        result.log = shader.getInfoLog();
        if (result.log.empty()) {
            result.log = "glslang 解析失败";
        }
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        result.log = program.getInfoLog();
        if (result.log.empty()) {
            result.log = "glslang 链接失败";
        }
        return false;
    }

    glslang::SpvOptions spvOptions;
    spvOptions.generateDebugInfo = options.generateDebugInfo;
    spvOptions.disableOptimizer = true;  // 未链接 SPIRV-Tools，跳过优化
    spvOptions.optimizeSize = false;
    glslang::GlslangToSpv(*program.getIntermediate(language), result.words, &spvOptions);

    if (result.words.empty() || result.words.front() != kSpirvMagic) {
        result.log = "SPIR-V 输出无效";
        result.words.clear();
        return false;
    }
    result.ok = true;
    return true;
}
#endif

}  // namespace

bool Compiler::SpirvBackendAvailable() {
#if defined(JZP_SHADER_WITH_GLSLANG)
    return true;
#else
    return false;
#endif
}

const char* Compiler::SpirvBackendName() {
#if defined(JZP_SHADER_WITH_GLSLANG)
    return "glslang";
#else
    return "none";
#endif
}

PreprocessResult Compiler::PrepareSource(std::string_view path, const CompileOptions& options) {
    return PreprocessFile(path, WithStageFromPath(options, path).preprocess);
}

PreprocessResult Compiler::PrepareSourceText(std::string_view source, std::string_view directory,
                                             const CompileOptions& options) {
    return PreprocessSource(source, directory, options.preprocess);
}

SpirvResult Compiler::CompileToSpirv(std::string_view path, const CompileOptions& options) {
    const CompileOptions effective = WithStageFromPath(options, path);
    const PreprocessResult prepared = PreprocessFile(path, effective.preprocess);
    if (!prepared.ok) {
        SpirvResult result;
        result.log = prepared.warnings.empty() ? "源码预处理失败" : prepared.warnings.front();
        return result;
    }
    return CompileTextToSpirv(prepared.source, std::string_view(), effective);
}

SpirvResult Compiler::CompileTextToSpirv(std::string_view source, std::string_view directory,
                                         const CompileOptions& options) {
    (void)directory;
    SpirvResult result;
#if defined(JZP_SHADER_WITH_GLSLANG)
    const PreprocessResult prepared = PreprocessSource(source, directory, options.preprocess);
    if (!prepared.ok) {
        result.log = prepared.warnings.empty() ? "源码预处理失败" : prepared.warnings.front();
        return result;
    }
    CompileWithGlslang(prepared.source, options.preprocess.stage, options, result);
    return result;
#else
    (void)source;
    result.log =
        "未启用 SPIR-V 后端：编译时定义 JZP_SHADER_WITH_GLSLANG 并链接 glslang（OpenGL 路径不受影响）";
    return result;
#endif
}

}  // namespace shader
