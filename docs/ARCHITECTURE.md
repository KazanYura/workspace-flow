# System Architecture

## Technology Stack
* **Language:** C++20
* **Target Platform:** Windows 10/11 (x64)
* **Build System:** CMake (Modern, target-based), MSVC (Visual Studio 2022) or clang-cl
* **Configuration:** `yaml-cpp`
* **UI Framework:** Dear ImGui (GLFW + OpenGL3 backend; DirectX 11 backend is an acceptable alternative on Windows)
* **Process Management:** Win32 API (`CreateProcessW`, anonymous pipes for stdout/stderr redirection, `WaitForSingleObject`, `TerminateProcess`)
* **System Telemetry:** WMI (`MSAcpi_ThermalZoneTemperature` in `root\WMI`) or the PDH performance-counter API, behind a sensor interface with a LibreHardwareMonitor fallback
* **Dependencies:** Pulled via CMake `FetchContent` (Dear ImGui, GLFW, yaml-cpp, spdlog) so the project builds without a system package manager

## Core Modules

### 1. Configuration Engine (`ConfigParser`)
Parses the user-defined pipeline configuration. Translates YAML nodes into an ordered, sequential list of `Task` objects plus the `ThermalMonitor` settings. Validation errors are surfaced as `std::optional`/`std::expected` results rather than exceptions.

### 2. Task Execution Pipeline (`PipelineRunner`)
A state machine that manages the lifecycle of tasks. There are exactly three task types:
* `command`: Spawns a process via `CreateProcessW` and waits for it to exit, capturing stdout/stderr (e.g., pulling a git repo).
* `poll`: Runs a readiness check on a loop until it succeeds or times out (e.g., connecting to the Docker Engine named pipe `\\.\pipe\docker_engine`, or running `docker info` and checking the exit code).
* `gate`: Emits an event to the UI thread, halting the pipeline until the user resolves it (e.g., approving the Cisco AnyConnect 2FA prompt).

### 3. Telemetry & Thermal Watchdog (`ThermalMonitor`)
A `std::jthread` running on a slow tick rate (e.g., every 5 seconds).
* Reads hardware sensors through the WMI/PDH sensor interface.
* Maintains a rolling average to prevent hysteresis bouncing around the threshold.
* On a breach, dispatches mitigation against configured target processes: suspend all threads (toolhelp snapshot + `SuspendThread`, or `NtSuspendProcess`) or terminate via `TerminateProcess`.

### 4. GUI Layer (`ImGuiController`) — post-MVP
The MVP is a console application: the `PipelineRunner` and `ThermalMonitor` report through a logging sink (spdlog) to stdout, and `gate` tasks pause for terminal input. The `ImGuiController` is layered on only after that console core is working end-to-end.

Once added, it renders the application state and runs on the main thread (required for OS windowing and the ImGui render loop):
* Reads state atomically from the `PipelineRunner` and `ThermalMonitor`.
* Displays the current active task, captured stdout/stderr logs from subprocesses, and a temperature graph.
* Provides the "Continue" button that resolves the Cisco AnyConnect interactive gate.

## Threading Model
* **Main Thread:** Handles window creation, the Win32 message loop, and Dear ImGui rendering.
* **Orchestrator Thread:** Runs the `PipelineRunner`. Communicates with the Main Thread via thread-safe queues (for logs) and `std::atomic` state flags (for task status and gate resolution).
* **Telemetry Thread:** Runs the `ThermalMonitor` as a `std::jthread` with cooperative cancellation via `std::stop_token`.
