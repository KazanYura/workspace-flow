---
name: add-fetchcontent-dependency
description: 'Add a third-party C++ dependency to DevFlow Orchestrator via CMake FetchContent (Dear ImGui, yaml-cpp, spdlog, GLFW). Use when wiring a new library, fixing FetchContent declarations, or making the project build on a fresh Windows machine without a system package manager.'
---

# Add a FetchContent Dependency

## When to Use
- Adding yaml-cpp, spdlog, Dear ImGui, GLFW, or any other library to the build.
- The project must build on a clean Windows box with only CMake + MSVC/clang-cl — no vcpkg/Conan/system packages.

## Procedure
1. **Pin a release.** Prefer a tagged commit/`GIT_TAG` over a branch for reproducibility.
2. **Declare and make available** in the top-level `CMakeLists.txt`:
   ```cmake
   include(FetchContent)

   FetchContent_Declare(
     yaml-cpp
     GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
     GIT_TAG        0.8.0
   )
   # Silence sub-project options you don't need before making it available:
   set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
   FetchContent_MakeAvailable(yaml-cpp)
   ```
3. **Link the target:**
   ```cmake
   target_link_libraries(devflow PRIVATE yaml-cpp spdlog::spdlog)
   ```
4. **Header-only / non-CMake libs (e.g., Dear ImGui):** fetch the source, then define your own target from its sources:
   ```cmake
   FetchContent_Declare(imgui
     GIT_REPOSITORY https://github.com/ocornut/imgui.git
     GIT_TAG        v1.91.5)
   FetchContent_MakeAvailable(imgui)
   add_library(imgui STATIC
     ${imgui_SOURCE_DIR}/imgui.cpp
     ${imgui_SOURCE_DIR}/imgui_draw.cpp
     ${imgui_SOURCE_DIR}/imgui_tables.cpp
     ${imgui_SOURCE_DIR}/imgui_widgets.cpp
     ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
     ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
   target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
   ```
5. **Configure & verify:** run `cmake -S . -B build` then `cmake --build build`. FetchContent downloads into `build/_deps/` on first configure.

## Guidance
- Set a dependency's `*_BUILD_TESTS`/`*_BUILD_EXAMPLES` options `OFF` **before** `FetchContent_MakeAvailable` to keep the build lean.
- Prefer the imported target name the library exports (`spdlog::spdlog`, `yaml-cpp`) over raw paths.
- Keep MVP-phase deps minimal: Phases 1–3 need only `yaml-cpp` and `spdlog`. Add ImGui/GLFW in Phase 4.
- Do not commit `build/`; FetchContent re-fetches on a clean configure.
