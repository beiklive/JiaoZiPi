cmake_minimum_required(VERSION 3.25)

project(JiaoZiPi VERSION 0.1.0 LANGUAGES CXX)

option(JIAOZIPI_BUILD_APP "Build the JiaoZiPi ImGui application" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/ui_manifest.json
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources)

set(BAOJIAOZI_BUILD_DESIGNER OFF CACHE BOOL "" FORCE)
set(BAOJIAOZI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/baojiaozi EXCLUDE_FROM_ALL)

if(JIAOZIPI_BUILD_APP)
    find_package(OpenGL REQUIRED)
    add_executable(jiaozi_pi_app
        src/app/main.cpp
    )
    target_compile_definitions(jiaozi_pi_app PRIVATE
        JIAOZIPI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        GL_SILENCE_DEPRECATION
    )
    target_link_libraries(jiaozi_pi_app PRIVATE
        baojiaozi_core
        baojiaozi_imgui
        OpenGL::GL
    )
endif()
