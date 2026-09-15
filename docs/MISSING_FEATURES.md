# Missing Features & Gaps

Analysis of DevFlow Orchestrator against its own docs ([ARCHITECTURE.md](ARCHITECTURE.md), [AI_CONTEXT.md](AI_CONTEXT.md), [ROADMAP.md](ROADMAP.md)) and current code in `include/devflow/` and `src/`. These are gaps considered important enough to prioritize before/alongside further GUI work. No in-code TODO/FIXME markers exist — this list is derived from architecture review, not code comments.

## Security

- **Shell commands remain opt-in trust boundaries** ([src/pipeline/Subprocess.cpp](../src/pipeline/Subprocess.cpp)): command and poll tasks now support `shell: false` for direct executable launch, but the default remains `shell: true` for compatibility with existing `.cmd` files and built-ins. Untrusted configs must explicitly use the non-shell mode.

## Configuration validation

`Config::validate()` now rejects duplicate IDs, invalid thermal threshold ordering, invalid retry intervals/counts, empty commands, empty gate messages, empty pipelines, and invalid pipeline timeout values. Remaining validation work includes tighter temperature bounds and command-path validation before execution.

Command, poll, gate pre-command, and failure-hook strings support `${VAR}` environment expansion. Undefined variables are rejected during parsing.

## Error handling & recovery

- Command retries, an optional detached `on_failure_command`, and an overall pipeline timeout are now supported.
- `continue_on_error: true` and the `--continue-on-error` CLI override allow later tasks to run after a failure while preserving an unsuccessful pipeline result.
- Command tasks now support optional `undo_command` actions, executed in reverse order for successful commands when the pipeline fails.
- `ProcessResult.error` now includes `FormatMessageW` text, but errors are still not structured.

## Logging & observability

- `spdlog` is fetched via CMake but not actually used anywhere in `devflow_core` — logging goes through the ad hoc `LogSink` only.
- File logging (`--log-file`) and optional thermal CSV history (`readings_csv_path`) are now supported.
- Task outcomes now expose structured task duration metadata; log levels, richer structured records, and a spdlog-backed core logger are still absent.

## GUI completeness (ROADMAP Phase 5, in progress)

- Live task-state sync from the pipeline thread into `DashboardState.tasks` is implemented in the current controller.
- Registry-based per-user auto-start is available through `--register-autostart` and `--unregister-autostart`.
- No system tray integration (`Shell_NotifyIcon`) or wake-from-sleep handling (`WM_POWERBROADCAST`). GLFW currently owns the window message loop, so these require a native message-window integration.

## Test coverage gaps

Covered today: `ConfigParser`, `Subprocess::run_process`, `PipelineRunner`, `ThermalMonitor`.

Not covered by any test:
- `Console` / `ConsoleLog` (ANSI color detection, thread-safe stdout sink)
- `CommandLine` (argv parsing, `--dry-run`)
- `ConfigPrinter`
- `ConsoleRunner` (interactive gate handling, exit code propagation)
- `ProcessControl` (suspend/resume/terminate by image name)
- `WmiTemperatureSensor` (COM/WMI query path, error handling)
- All `Gui*` classes (`GuiApp`, `Dashboard`, `GuiPipelineController`)
