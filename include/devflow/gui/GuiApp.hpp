#pragma once

#include <functional>
#include <memory>
#include <string>

#include "devflow/gui/DashboardState.hpp"

namespace devflow::gui {

// Owns the GLFW window, its OpenGL context, and the Dear ImGui context, all via
// RAII (see the pimpl in the .cpp). The render loop runs on the calling thread,
// which must be the main thread because OS windowing and GL require it.
//
// GLFW drives the Win32 message loop internally (glfwPollEvents), so this class
// keeps all windowing details out of the header and away from the system layer.
class GuiApp {
public:
    // Invoked once per frame before rendering so the caller can refresh the
    // DashboardState (from live threads in Phase 5).
    using FrameCallback = std::function<void(DashboardState&)>;

    GuiApp(const std::string& title, int width, int height);
    ~GuiApp();

    GuiApp(const GuiApp&) = delete;
    GuiApp& operator=(const GuiApp&) = delete;

    // True only if the window, GL context, and ImGui backends all initialized.
    [[nodiscard]] bool valid() const noexcept;

    // Runs the render loop until the window is closed. No-op if !valid().
    void run(const FrameCallback& on_frame);

    [[nodiscard]] DashboardState& state() noexcept { return state_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;  // hides GLFW/ImGui from consumers of this header
    DashboardState state_;
};

}  // namespace devflow::gui
