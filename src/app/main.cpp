#include "baojiaozi/imgui/renderer.hpp"
#include "baojiaozi/parser/project_loader.hpp"
#include "baojiaozi/runtime/runtime.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <GLFW/glfw3.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

constexpr const char* kGlslVersion = "#version 150";

std::filesystem::path DefaultThemeDirectory() {
    return std::filesystem::path(JIAOZIPI_SOURCE_DIR) / "resources/themes/default_theme";
}

std::filesystem::path ThemeDirectoryFromArguments(int argc, char** argv) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::string(argv[index]) == "--theme-dir") return argv[index + 1];
    }
    return DefaultThemeDirectory();
}

bool LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const auto font = std::filesystem::path(JIAOZIPI_SOURCE_DIR) /
                      "third_party/baojiaozi/resources/fonts/switch_font.ttf";
    if (!std::filesystem::is_regular_file(font)) return false;
    ImFont* defaultFont = io.Fonts->AddFontFromFileTTF(
        font.string().c_str(), 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    if (defaultFont != nullptr) io.FontDefault = defaultFont;
    return defaultFont != nullptr;
}

} // namespace

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::cerr << "JiaoZiPi: GLFW initialization failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "JiaoZiPi", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(kGlslVersion);
    const bool fontsLoaded = LoadFonts();

    baojiaozi::parser::ProjectLoader loader;
    auto projectResult = loader.LoadDirectory(ThemeDirectoryFromArguments(argc, argv));
    if (!projectResult.Succeeded()) {
        for (const auto& diagnostic : projectResult.diagnostics) {
            std::cerr << diagnostic.source << diagnostic.path << ": " << diagnostic.message << '\n';
        }
    }
    const auto project = projectResult.project;
    baojiaozi::imgui::Renderer renderer;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("项目")) {
                ImGui::MenuItem("主题目录");
                ImGui::MenuItem("退出");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("视图")) ImGui::EndMenu();
            if (ImGui::BeginMenu("设置")) ImGui::EndMenu();
            ImGui::EndMainMenuBar();
        }

        ImGui::Begin("JiaoZiPi");
        ImGui::TextUnformatted("JiaoZiPi 多核心模拟器前端");
        ImGui::Text("主题字体: %s", fontsLoaded ? "switch_font.ttf" : "系统回退字体");
        ImGui::Separator();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 viewport = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("home_surface", viewport);
        if (project) {
            baojiaozi::runtime::Runtime runtime(*project);
            const auto home = runtime.BuildPage("home", {0.0f, 0.0f, viewport.x, viewport.y});
            renderer.Render(home.root, origin);
        } else {
            ImGui::TextUnformatted("无法加载主页主题项目，请检查主题目录。");
        }
        ImGui::End();

        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
