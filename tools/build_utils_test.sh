#!/bin/sh
# 构建并运行 src/utils 模块冒烟测试。
#
# 首次使用需拉取子模块：git submodule update --init --recursive
#
# 额外验证 Vulkan/SPIR-V 路径（可选）：
#   GLSLANG_INCLUDE=<glslang 源码或安装的 include 根> \
#   GLSLANG_LIB=<glslang 构建目录（含 glslang/、SPIRV/ 子目录）> \
#   sh tools/build_utils_test.sh
set -e

cd "$(dirname "$0")/.."

CXX="${CXX:-c++}"
INCLUDES="-I src/utils -I third_party/spdlog/include -I third_party/json/include"
LIBS=""

if [ -n "$GLSLANG_INCLUDE" ]; then
    INCLUDES="$INCLUDES -I $GLSLANG_INCLUDE -DJZP_SHADER_WITH_GLSLANG"
fi

if [ -n "$GLSLANG_LIB" ]; then
    LIBS="-L $GLSLANG_LIB/glslang -L $GLSLANG_LIB/SPIRV -L $GLSLANG_LIB/glslang/OSDependent/Unix"
    LIBS="$LIBS -lglslang -lMachineIndependent -lGenericCodeGen -lSPIRV -lOSDependent"
    LIBS="$LIBS -lglslang-default-resource-limits"
fi

# shellcheck disable=SC2086
"$CXX" -std=c++17 -Wall -Wextra -O1 $INCLUDES \
    src/utils/log/Logger.cpp \
    src/utils/paths/DataPaths.cpp \
    src/utils/i18n/I18n.cpp \
    src/utils/cheats/ChtFile.cpp \
    src/utils/shader/ShaderPreset.cpp \
    src/utils/shader/ShaderSource.cpp \
    src/utils/shader/ShaderCompiler.cpp \
    tests/utils_smoke.cpp \
    $LIBS \
    -o tests/utils_smoke

./tests/utils_smoke
