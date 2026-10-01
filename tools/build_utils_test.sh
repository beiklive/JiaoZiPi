#!/bin/sh
# 构建并运行 src/utils 模块（log / i18n）冒烟测试。
# 首次使用需拉取子模块：git submodule update --init --recursive
set -e

cd "$(dirname "$0")/.."

CXX="${CXX:-c++}"

"$CXX" -std=c++17 -Wall -Wextra -O1 \
    -I src/utils \
    -I third_party/spdlog/include \
    -I third_party/json/include \
    src/utils/log/Logger.cpp \
    src/utils/cheats/ChtFile.cpp \
    src/utils/paths/DataPaths.cpp \
    src/utils/i18n/I18n.cpp \
    tests/utils_smoke.cpp \
    -o tests/utils_smoke

./tests/utils_smoke
