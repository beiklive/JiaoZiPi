#pragma once

// 着色器源码预处理（独立模块，不依赖宿主项目、不依赖图形 API）
//
// - 展开 `#include "..."`（相对当前文件，再查 include 目录；带深度与循环保护）
// - 收集 `#pragma parameter NAME "说明" 初始值 最小值 最大值 [步长]`
// - 注入 `#version` / `#define VERTEX|FRAGMENT` / 参数 `#define` / `PARAMETER_UNIFORM`
// - 按需剔除 `#pragma parameter|stage|name|format` 行（RetroArch 同样会剔除）
//
// 产出的源码可直接给 OpenGL 的 glShaderSource，或交给 ShaderCompiler 编译成 SPIR-V。

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shader {

enum class Stage { Vertex, Fragment, Compute, Unknown };

struct ParameterSpec {
    std::string name;
    std::string description;
    float initial = 0.0f;
    float minimum = 0.0f;
    float maximum = 0.0f;
    float step = 0.0f;
    bool hasStep = false;
};

struct PreprocessOptions {
    Stage stage = Stage::Fragment;
    std::string version;                        // "450" / "300 es"；为空则不注入 #version
    std::vector<std::string> defines;           // 额外 #define
    std::vector<std::pair<std::string, float>> parameterValues;  // 注入 #define NAME <值>
    bool defineStage = true;                    // #define VERTEX / FRAGMENT
    bool defineParameterUniform = true;         // 有参数时 #define PARAMETER_UNIFORM
    bool stripPragmas = true;                   // 去掉 #pragma parameter/stage/name/format
    std::vector<std::string> includeDirectories;
    int maxIncludeDepth = 16;
};

struct PreprocessResult {
    bool ok = false;
    std::string source;
    std::vector<ParameterSpec> parameters;
    std::vector<std::string> warnings;
};

Stage StageFromPath(std::string_view path);
const char* StageDefine(Stage stage);

// 只收集参数，不做预处理
bool CollectParameters(std::string_view source, std::vector<ParameterSpec>& out,
                       std::vector<std::string>& warnings);

PreprocessResult PreprocessSource(std::string_view source, std::string_view directory,
                                  const PreprocessOptions& options);
PreprocessResult PreprocessFile(std::string_view path, const PreprocessOptions& options);

}  // namespace shader
