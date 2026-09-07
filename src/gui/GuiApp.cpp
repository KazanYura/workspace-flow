#include "devflow/gui/GuiApp.hpp"

#include <cstdio>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include "devflow/gui/Dashboard.hpp"

namespace devflow::gui {
namespace {

// GLSL 1.30 pairs with an OpenGL 3.0 context: the most portable combination the
// Dear ImGui OpenGL3 backend supports across the Intel/AMD/NVIDIA drivers on the
// dev machines this tool targets.
constexpr const char* kGlslVersion = "#version 130";

void glfw_error_callback(int code, const char* description) {
    std::fprintf(stderr, "[glfw] error %d: %s\n", code, description);
}

}  // namespace

// Holds every raw resource so ~Impl tears them down in reverse init order; the
// GuiApp itself never touches a raw GLFW/ImGui handle.
struct GuiApp::Impl {
    bool glfw_ready = false;
    GLFWwindow* window = nullptr;
    bool imgui_ready = false;

    ~Impl() {
        if (imgui_ready) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
        if (glfw_ready) {
            glfwTerminate();
        }
    }
};

GuiApp::GuiApp(const std::string& title, int width, int height)
    : impl_(std::make_unique<Impl>()) {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() == GLFW_FALSE) {
        return;
    }
    impl_->glfw_ready = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    impl_->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (impl_->window == nullptr) {
        return;
    }
    glfwMakeContextCurrent(impl_->window);
    glfwSwapInterval(1);  // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // don't litter the cwd with imgui.ini
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(impl_->window, true)) {
        return;
    }
    if (!ImGui_ImplOpenGL3_Init(kGlslVersion)) {
        return;
    }
    impl_->imgui_ready = true;
}

GuiApp::~GuiApp() = default;

bool GuiApp::valid() const noexcept {
    return impl_ && impl_->window != nullptr && impl_->imgui_ready;
}

void GuiApp::run(const FrameCallback& on_frame) {
    if (!valid()) {
        return;
    }

    while (glfwWindowShouldClose(impl_->window) == GLFW_FALSE) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (on_frame) {
            on_frame(state_);
        }
        render_dashboard(state_);

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(impl_->window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(impl_->window);
    }
}

}  // namespace devflow::gui
