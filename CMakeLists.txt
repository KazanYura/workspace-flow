cmake_minimum_required(VERSION 3.20)
project(DevFlowOrchestrator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # /utf-8: treat sources as UTF-8. /permissive-: strict conformance.
  add_compile_options(/utf-8 /W4 /permissive-)
endif()

find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        0.8.0
)
# Trim the sub-project down before making it available.
set(YAML_CPP_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
# yaml-cpp 0.8.0 declares cmake_minimum_required < 3.5, which CMake >= 4.0 rejects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(yaml-cpp)

# yaml-cpp 0.8.0's emitterutils.cpp uses uint16_t without including <cstdint>,
# which fails to compile under GCC 13+/libstdc++. Force-include it instead of
# patching the fetched source (which a clean checkout would wipe).
if(TARGET yaml-cpp)
  if(MSVC)
    target_compile_options(yaml-cpp PRIVATE /FIcstdint)
  else()
    target_compile_options(yaml-cpp PRIVATE "SHELL:-include cstdint")
  endif()
endif()

# Core: config model, parser, the Phase 2 execution engine, and the Phase 3
# thermal watchdog (sensor + process control + monitor daemon).
add_library(devflow_core STATIC
  src/config/ConfigParser.cpp
  src/pipeline/Subprocess.cpp
  src/pipeline/PipelineRunner.cpp
  src/thermal/WmiTemperatureSensor.cpp
  src/thermal/ProcessControl.cpp
  src/thermal/ThermalMonitor.cpp
)
target_include_directories(devflow_core PUBLIC include)
target_link_libraries(devflow_core PUBLIC yaml-cpp Threads::Threads)
target_compile_features(devflow_core PUBLIC cxx_std_20)

# WMI (COM) for the thermal sensor and toolhelp for process mitigation.
if(WIN32)
  target_link_libraries(devflow_core PUBLIC wbemuuid ole32 oleaut32)
endif()

# Console application (Phase 1 deliverable).
add_executable(devflow
  src/main.cpp
  src/console/CommandLine.cpp
  src/console/ConfigPrinter.cpp
  src/console/ConsoleRunner.cpp
  src/console/ConsoleLog.cpp
  src/console/Console.cpp
  include/devflow/console/CommandLine.hpp
  include/devflow/console/ConfigPrinter.hpp
  include/devflow/console/ConsoleRunner.hpp
  include/devflow/console/ConsoleLog.hpp
  include/devflow/console/Console.hpp
)
target_link_libraries(devflow PRIVATE devflow_core)

# ---------------------------------------------------------------------------
# Phase 4: Dear ImGui GUI frontend (GLFW + OpenGL3 backend).
# Optional so the console MVP still builds without fetching the GUI deps.
# ---------------------------------------------------------------------------
option(DEVFLOW_BUILD_GUI "Build the Dear ImGui GUI frontend (Phase 4)" ON)

if(DEVFLOW_BUILD_GUI)
  find_package(OpenGL REQUIRED)

  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
  )
  set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

  # Dear ImGui ships no CMakeLists, so MakeAvailable only populates the source.
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.9
  )

  FetchContent_MakeAvailable(glfw imgui)

  # Build ImGui core plus the GLFW + OpenGL3 backends as one static library.
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

  # GUI rendering layer plus the controller that bridges it to the engine. The
  # renderer stays ImGui-only; the controller owns all engine/thread wiring.
  add_library(devflow_ui STATIC
    src/gui/GuiApp.cpp
    src/gui/Dashboard.cpp
    src/gui/GuiPipelineController.cpp
    include/devflow/gui/GuiApp.hpp
    include/devflow/gui/Dashboard.hpp
    include/devflow/gui/DashboardState.hpp
    include/devflow/gui/GuiPipelineController.hpp
  )
  target_include_directories(devflow_ui PUBLIC include)
  target_link_libraries(devflow_ui PUBLIC imgui devflow_core)
  target_compile_features(devflow_ui PUBLIC cxx_std_20)
  if(WIN32)
    target_link_libraries(devflow_ui PUBLIC comdlg32)  # GetOpenFileNameW
  endif()

  add_executable(devflow_gui src/gui/gui_main.cpp)
  target_link_libraries(devflow_gui PRIVATE devflow_ui)
endif()

enable_testing()
add_subdirectory(tests)
