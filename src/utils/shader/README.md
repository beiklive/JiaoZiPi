# shader —— 着色器预设解析 + 编译（可整包搬进核心）

独立模块：不依赖宿主项目、不依赖图形 API、不依赖任何第三方（Vulkan/SPIR-V 路径的 glslang 是**可选**的）。
设计目标就是「复制 `shader/` 这一个目录，就能塞进任意模拟器核心」。

## 三层职责

| 层 | 文件 | 做什么 | 依赖 |
| --- | --- | --- | --- |
| 预设解析 | `ShaderPreset.h/.cpp` | 读 `.slangp` / `.glslp` / `.cgp`：pass 链、缩放、过滤、纹理、参数、`#reference` 继承 | 无 |
| 源码预处理 | `ShaderSource.h/.cpp` | `#include` 展开、`#pragma parameter` 收集、注入 `#version`/stage/参数宏、剔除 pragma | 无 |
| 编译 | `ShaderCompiler.h/.cpp` | OpenGL 路径产出最终 GLSL；Vulkan 路径用 glslang 编出 SPIR-V | glslang 可选 |

不做（保持低耦合）：不创建 GPU 资源、不做 FBO/管线/渲染循环、不做 SPIRV-Cross 反射、不含 Cg 后端。

## 预设格式（RetroArch 兼容）

```ini
#reference "base.slangp"          # 继承：被引用的先合并，本文件的值覆盖它

shaders = 2
shader0 = "shaders/scanline.slang"
filter_linear0 = false            # true=Linear / false=Nearest
wrap_mode0 = clamp_to_edge        # clamp_to_border | clamp_to_edge | repeat | mirrored_repeat
scale_type0 = viewport            # source | viewport | absolute
scale0 = 2.0
scale_type_x0 = absolute          # 单轴覆盖 scale_type0
scale_x0 = 320
scale_y0 = 240
frame_count_mod0 = 0
mipmap_input0 = false
srgb_framebuffer0 = false
float_framebuffer0 = false
rgb10_framebuffer0 = false
alias0 = SCANLINE

textures = "LUT"
LUT = "textures/lut.png"
LUT_linear = true
LUT_mipmap = false
LUT_wrap_mode = repeat

parameters = "SCANLINE_OPACITY"
SCANLINE_OPACITY = 0.35
```

- 路径相对预设文件所在目录解析；支持通配符替换（`Options::wildcards`，如 `{"$CORE$", "mgba"}`）
- `#reference` 支持多层，带深度上限（默认 16）与循环检测，链可在 `references()` 里查
- 参数**定义**来自着色器源码的 `#pragma parameter`（含 `#include` 进去的文件），预设只提供值
- 解析容错项（BOM、CRLF、注释、缺 `shaders` 键、pass 缺号、纹理缺路径）都会进 `Warnings()`

## 源码预处理

```glsl
#version 450
#include "helper.inc"
#pragma parameter BRIGHTNESS "Brightness" 1.0 0.0 2.0 0.01
```

`PreprocessFile()` 产出：

```glsl
#version 450
#define FRAGMENT 1
#define PARAMETER_UNIFORM 1
#define BRIGHTNESS 1.0

/* helper.inc 的内容（其 #pragma parameter 已被收集） */
```

要点：`#version` 必须是第一条有效语句，所以注入的 `#define` 会**插到它后面**（源码没有 `#version` 时才由 `options.version` 生成）。

## 编译

```cpp
#include "shader/ShaderPreset.h"
#include "shader/ShaderCompiler.h"

shader::Preset preset;
preset.Load("presets/crt.slangp");

shader::CompileOptions options;
options.preprocess.version = "450";
options.preprocess.stage = shader::Stage::Fragment;
options.preprocess.parameterValues = {{"BRIGHTNESS", 1.2f}};

// OpenGL / GLES：拿到源码，自己 glShaderSource + glCompileShader
shader::PreprocessResult glsl = shader::Compiler::PrepareSource(preset.passes()[0].source, options);

// Vulkan：GLSL → SPIR-V（需要 glslang 后端）
shader::SpirvResult spirv = shader::Compiler::CompileToSpirv(preset.passes()[0].source, options);
if (!spirv.ok) {
    // spirv.log 里有 glslang 的编译日志
}
```

开启 Vulkan 路径：

```bash
# 1) 编译 glslang（只要一次，链接进核心即可）
git clone --depth 1 https://github.com/KhronosGroup/glslang
cmake -S glslang -B build_glslang -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build build_glslang -j8

# 2) 编译时定义宏 + 带上头文件与库
c++ -std=c++17 -DJZP_SHADER_WITH_GLSLANG -I <glslang 根> ... \
    -L build_glslang/glslang -L build_glslang/SPIRV -L build_glslang/glslang/OSDependent/Unix \
    -lglslang -lMachineIndependent -lGenericCodeGen -lSPIRV -lOSDependent -lglslang-default-resource-limits
```

未定义该宏时 `Compiler::SpirvBackendAvailable()` 返回 `false`，`CompileToSpirv()` 返回带说明的失败，OpenGL 路径完全不受影响。
`-DENABLE_OPT=OFF` 是为了不依赖 SPIRV-Tools；代价是 SPIR-V 不做运行期优化（`disableOptimizer = true`）。

## API

| 分类 | 接口 |
| --- | --- |
| 预设 | `Preset::Load` / `LoadFromString` / `ResolveParameters`、`passes()`、`textures()`、`parameters()`、`references()`、`FindParameter`、`SetParameterValue`、`ResolvePath`、`FormatFromExtension` |
| 预处理 | `PreprocessFile` / `PreprocessSource` / `CollectParameters` / `StageFromPath` / `StageDefine` |
| 编译 | `Compiler::PrepareSource` / `PrepareSourceText` / `CompileToSpirv` / `CompileTextToSpirv` / `SpirvBackendAvailable` / `SpirvBackendName` |
| 常量 | `kSpirvMagic`、`Stage`、`ScaleType`、`WrapMode`、`FilterMode`、`Format` |
| 诊断 | `LastError()`、`Warnings()`、`SpirvResult::log`、`PreprocessResult::warnings` |

## 迁移进核心

1. 复制 `shader/` 目录（4 个文件 + 本 README），加进核心的构建；
2. 命名空间若与宿主冲突，全局替换 `namespace shader` 即可；
3. 需要 Vulkan 时再引入 glslang（各核心自带，本模块不强制）。

```bash
c++ -std=c++17 -I <包含 shader/ 的上级目录> shader/ShaderPreset.cpp shader/ShaderSource.cpp \
    shader/ShaderCompiler.cpp your_backend.cpp -o your_core
```

## 限制（先说清楚）

- 只做 GLSL/Slang 路线的**解析 + 编译**；Cg（`.cgp`）预设能解析，但没有 Cg 编译后端（现代平台用不到）
- 不做 SPIR-V 反射：UBO 布局/绑定号要么用 SPIRV-Cross（RA 的做法），要么在着色器里写成显式 `layout(std140, binding = N)`
- 不做 SPIR-V 缓存（`slang_cache.c` 那套），需要的话在宿主里按文件哈希缓存 `words`
- 预设里的 `$CONTENT-DIR$` / `$VID-DRV$` 等 RA 专用通配符不会自动解析，用 `Options::wildcards` 自己喂
