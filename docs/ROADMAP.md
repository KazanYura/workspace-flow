# Implementation Roadmap

To build this project efficiently using AI assistance, implement the features strictly in the following phases. Do not move to the next phase until the current one is tested and compiling. All phases target Windows 10/11 (x64) with the MSVC toolset.

## Phase 1: Project Skeleton & Configuration (CLI Only)
1. Initialize a modern CMake project structure (`src/`, `include/`, `tests/`) targeting MSVC/clang-cl.
2. Integrate `yaml-cpp` via `FetchContent`.
3. Define the C++ data structures representing the Pipeline, the three task types (`command`, `poll`, `gate`), and the thermal thresholds.
4. Write a parser that reads `config.yaml` and prints the parsed tasks to the console.

## Phase 2: Core Execution Engine (CLI Only)
1. Implement a `Subprocess` wrapper around `CreateProcessW` that runs a command and captures stdout/stderr via anonymous pipes. Wrap every `HANDLE` in an RAII type.
2. Create the `PipelineRunner` state machine.
3. Implement `command` (execute and wait on `WaitForSingleObject`) and `poll` (execute in a loop until success or timeout).
4. Implement a terminal-based `gate` task (wait for the user to press Enter).
5. Test the entire 6-step flow using dummy `.cmd`/PowerShell scripts.

## Phase 3: Thermal Monitor Daemon (CLI Only)
1. Write a WMI sensor reader (`MSAcpi_ThermalZoneTemperature` in `root\WMI`) behind a sensor interface, with a PDH or LibreHardwareMonitor fallback for hardware where WMI thermal zones are unavailable.
2. Implement a background loop (`std::jthread` with `std::stop_token`) that samples temperature and maintains a rolling average.
3. Implement the mitigation action: read the target process list from YAML and suspend (toolhelp snapshot + `SuspendThread`) or terminate (`TerminateProcess`) those processes.
4. Add console logging (spdlog) to verify it works under load.

> **✅ MVP milestone.** At the end of Phase 3 the project is a fully functional **console application with logs**: it runs the entire 6-step pipeline, gates on user input at the terminal, and applies thermal mitigation. Ship and dogfood from here before starting the GUI. Phases 4–5 add the Dear ImGui frontend on top of this working core.

## Phase 4: Dear ImGui Integration
1. Add Dear ImGui, GLFW, and an OpenGL3 backend (or the DirectX 11 backend) to CMake via `FetchContent`.
2. Create a basic desktop window with a Win32 message loop.
3. Design a simple UI dashboard showing:
    * A list of tasks (with color-coded states: Pending, Running, Done, Failed).
    * A live-updating text box for subprocess logs.
    * A modal popup for the "Cisco AnyConnect Gate".
    * A small widget showing the current CPU temperature.

## Phase 5: Thread Integration & Polish
1. Connect the CLI execution engine and thermal monitor to the Dear ImGui frontend.
2. Ensure thread safety (mutex-guarded log queues, atomics for task state and gate resolution).
3. Add a Windows system tray icon (`Shell_NotifyIcon`) so the app can hide in the background.
4. Add auto-start via the `Run` registry key or Task Scheduler, plus a wake-from-sleep hook (`WM_POWERBROADCAST`).

## Phase 6 candidates
See [MISSING_FEATURES.md](MISSING_FEATURES.md) and [NICE_TO_HAVE.md](NICE_TO_HAVE.md) for gaps and enhancements to consider once Phase 5 is complete.
