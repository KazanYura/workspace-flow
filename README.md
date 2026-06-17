# WorkspaceFlow — Design Specification

---

## 1. Overview

WorkspaceFlow is a lightweight, local-first Windows desktop application that lets users define named "workspaces" — groups of applications, folders, and URLs — and launch them all with a single click. It is built with C++17/20, Qt 6 (QtWidgets), and stores configuration locally as JSON. There is no cloud component, no telemetry, and no account system.

---

## 2. Goals and Non-Goals

### Goals

- Let users create, edit, delete, and launch named workspace profiles
- Launch `.exe` applications, Windows Explorer folders, and browser URLs simultaneously
- Keep the UI responsive during multi-target launches
- Store all data locally in a single JSON file with corruption-safe writes
- Handle real-world Windows paths (spaces, long paths, Unicode)

### Non-Goals

- Scheduling or timed launches
- Cross-platform support (Windows-only)
- Plugin or scripting system
- System tray / background daemon mode
- Auto-update mechanism

---

## 3. Architecture

The system follows a layered architecture with four distinct units. Dependencies flow strictly downward — UI depends on Core, Core depends on Storage and OS, and Storage and OS depend on nothing internal.

```
┌─────────────────────────────────┐
│           ui/                   │  Qt widgets, signals/slots
│  MainWindow, WorkspaceEditor   │
└──────────┬──────────────────────┘
           │ calls
┌──────────▼──────────────────────┐
│          core/                  │  Domain models, orchestration
│  Workspace, WorkspaceManager    │
└──────┬─────────────┬────────────┘
       │ calls       │ calls
┌──────▼──────┐ ┌────▼────────────┐
│  storage/   │ │      os/        │  Platform-specific execution
│  JsonStore  │ │  ProcessLauncher│
└─────────────┘ └─────────────────┘
```

Each layer has one clear responsibility:

| Layer | Responsibility | Public surface |
|-------|---------------|----------------|
| **ui/** | Render state, collect user input, display errors | Qt widget classes |
| **core/** | Own the workspace model, coordinate save/load/launch | `WorkspaceManager` API |
| **storage/** | Serialize/deserialize workspaces to JSON, atomic file writes | `JsonStore` API |
| **os/** | Spawn processes, open folders, open URLs via Windows API | `ProcessLauncher` API |

---

## 4. Data Model

### 4.1 Workspace

A workspace is a named collection of launch targets.

```cpp
struct LaunchTarget {
    enum class Kind { App, Folder, Url };

    Kind        kind;          // discriminator
    std::string path;          // exe path, folder path, or URL
    std::string args;          // command-line args (App only, empty otherwise)
    std::string display_name;  // optional human label
};

struct Workspace {
    std::string                id;       // UUID v4, assigned on creation
    std::string                name;     // user-visible name
    std::vector<LaunchTarget>  targets;
};
```

- `id` is a UUID string generated once at creation time. It is the stable identifier — `name` can be renamed freely.
- `args` is only meaningful for `Kind::App`. The UI disables the field for other kinds.
- `display_name` is optional. When empty, the UI derives a label from the path (filename or URL).

### 4.2 JSON Schema

All workspaces live in a single file. Default location: `%APPDATA%/WorkspaceFlow/config.json`.

```json
{
  "version": 1,
  "workspaces": [
    {
      "id": "a1b2c3d4-...",
      "name": "Morning Dev",
      "targets": [
        {
          "kind": "app",
          "path": "C:\\Program Files\\VSCode\\Code.exe",
          "args": "--new-window",
          "display_name": "VS Code"
        },
        {
          "kind": "folder",
          "path": "C:\\Projects\\my-repo",
          "args": "",
          "display_name": ""
        },
        {
          "kind": "url",
          "path": "https://github.com",
          "args": "",
          "display_name": "GitHub"
        }
      ]
    }
  ]
}
```

The `version` field exists so future releases can migrate the schema without breaking existing files. Version `1` is the only version defined in this spec.

---

## 5. Component Designs

### 5.1 `storage::JsonStore`

**What it does:** Reads and writes the workspace list to the JSON config file.

**Public interface:**

```cpp
class JsonStore {
public:
    explicit JsonStore(std::filesystem::path config_path);

    // Load all workspaces from disk. Returns empty vector if file
    // does not exist (first run). Throws on parse error.
    std::vector<core::Workspace> load();

    // Persist the full workspace list to disk atomically.
    void save(const std::vector<core::Workspace>& workspaces);
};
```

**Atomic writes:** `save()` writes to a temporary file (`config.json.tmp`) in the same directory, flushes, then calls `std::filesystem::rename()` to atomically replace the original. On Windows, `rename` over an existing file works on NTFS. If the process crashes mid-write, only the `.tmp` file is corrupt — the original `config.json` remains intact.

**First-run behavior:** If `config.json` does not exist, `load()` returns an empty vector. The directory is created on the first `save()` call.

**Error handling:** Parse failures (malformed JSON, missing required fields) throw a descriptive `std::runtime_error`. The caller (`WorkspaceManager`) catches this and surfaces it to the UI.

### 5.2 `os::ProcessLauncher`

**What it does:** Launches a single target using the Windows API. One function, three code paths based on `Kind`.

**Public interface:**

```cpp
struct LaunchResult {
    bool        success;
    std::string error_message;  // empty on success
};

class ProcessLauncher {
public:
    // Launch a single target. Does not block.
    // Returns immediately with success/failure status.
    LaunchResult launch(const core::LaunchTarget& target);
};
```

**Implementation strategy per kind:**

| Kind | API | Notes |
|------|-----|-------|
| `App` | `CreateProcessW` | Preferred over `ShellExecuteEx` for direct `.exe` launches — gives a process handle and clear error codes. Paths with spaces are safe because `CreateProcessW` takes the application path and argument string as separate wide-string parameters. |
| `Folder` | `ShellExecuteExW` with verb `"explore"` | Opens Windows Explorer at the given path. |
| `Url` | `ShellExecuteExW` with verb `"open"` | Opens the URL in the user's default browser. |

**Space handling:** All path strings are converted to `std::wstring` (UTF-16) before passing to Windows APIs. `CreateProcessW` receives the executable path as `lpApplicationName` (quoted) and arguments as `lpCommandLine`, avoiding the classic space-in-path bug.

**Detached processes:** For `CreateProcessW`, the `DETACHED_PROCESS` and `CREATE_NEW_PROCESS_GROUP` flags are set so spawned processes are independent of WorkspaceFlow's lifetime. Process and thread handles are closed immediately after launch.

**Error handling:** If the API call fails, `LaunchResult` captures the error via `GetLastError()` formatted with `FormatMessageW`. The caller logs it and continues launching remaining targets.

### 5.3 `core::WorkspaceManager`

**What it does:** Owns the in-memory workspace list. Coordinates CRUD operations (delegating persistence to `JsonStore`) and batch launches (delegating execution to `ProcessLauncher`).

**Public interface:**

```cpp
class WorkspaceManager {
public:
    WorkspaceManager(storage::JsonStore& store,
                     os::ProcessLauncher& launcher);

    void load_from_disk();   // populate in-memory list
    void save_to_disk();     // persist current state

    const std::vector<Workspace>& workspaces() const;

    void add_workspace(Workspace ws);
    void update_workspace(const std::string& id, Workspace ws);
    void remove_workspace(const std::string& id);

    // Launch all targets in a workspace. Returns per-target results.
    std::vector<os::LaunchResult> launch(const std::string& workspace_id);
};
```

**Launch behavior:** `launch()` iterates through the workspace's targets sequentially on a worker thread (see §5.4). Each target is passed to `ProcessLauncher::launch()`. Results are collected and returned. A failure on one target does not stop the remaining targets.

**ID generation:** `add_workspace()` assigns a UUID v4 string using a simple random generator (`std::mt19937` seeded from `std::random_device`, formatted as 8-4-4-4-12 hex). No external UUID library needed.

### 5.4 `ui/` — Qt Widgets

**What it does:** Presents the workspace list and editor forms. Dispatches work to `WorkspaceManager`.

#### Main Window (`MainWindow`)

- A `QListWidget` on the left showing workspace names
- Buttons: **New**, **Edit**, **Delete**, **Launch**
- **Launch** is the primary action — prominent button, also double-click on list item
- A status bar at the bottom showing launch results or errors

#### Workspace Editor (`WorkspaceEditorDialog`)

- A modal `QDialog` opened for New / Edit
- Fields: workspace name (text input), target list (table widget)
- Target table columns: Kind (combo box: App / Folder / URL), Path (text input + browse button for App/Folder), Args (text input, disabled for non-App), Display Name (text input, optional)
- Browse button opens `QFileDialog::getOpenFileName` for App or `QFileDialog::getExistingDirectory` for Folder
- Add / Remove target buttons below the table
- OK / Cancel buttons

#### Async Launch

Launching must not freeze the UI. Implementation:

1. `MainWindow::on_launch_clicked()` disables the Launch button and shows "Launching…" in the status bar.
2. A `QtConcurrent::run()` call executes `WorkspaceManager::launch()` on a thread pool thread.
3. A `QFutureWatcher` signals `finished()` back on the UI thread.
4. The UI re-enables the button and displays results: success count, and any errors as a `QMessageBox::warning` listing the failed targets and their error messages.

This avoids manual thread management. `QtConcurrent` is part of Qt 6 and sufficient for this fire-and-forget pattern.

---

## 6. Error Handling Strategy

| Error scenario | Behavior |
|---|---|
| Config file missing (first run) | `JsonStore::load()` returns empty list. App starts with no workspaces. |
| Config file malformed JSON | `WorkspaceManager::load_from_disk()` catches exception, shows `QMessageBox::critical` with details. App starts with empty list so user can recreate workspaces. |
| Target path does not exist / exe not found | `ProcessLauncher::launch()` returns `LaunchResult{false, "..."}`. Remaining targets still launch. Summary shown in message box after batch completes. |
| Disk full / write failure on save | `JsonStore::save()` throws. `WorkspaceManager::save_to_disk()` catches and propagates to UI as `QMessageBox::critical`. In-memory state is still valid — user can retry or fix disk. |
| Crash during save | Atomic write strategy (§5.1) ensures `config.json` is never partially written. |

---

## 7. File Layout

```
WorkspaceFlow/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                      # QApplication bootstrap, load, show
│   ├── core/
│   │   ├── workspace.h               # Workspace, LaunchTarget structs
│   │   ├── workspace_manager.h
│   │   └── workspace_manager.cpp
│   ├── storage/
│   │   ├── json_store.h
│   │   └── json_store.cpp
│   ├── os/
│   │   ├── process_launcher.h        # LaunchResult struct + class
│   │   └── process_launcher.cpp
│   └── ui/
│       ├── main_window.h
│       ├── main_window.cpp
│       ├── workspace_editor_dialog.h
│       └── workspace_editor_dialog.cpp
└── third_party/
    └── nlohmann/
        └── json.hpp                   # single-header, vendored
```

---

## 8. Build Configuration (CMake)

```cmake
cmake_minimum_required(VERSION 3.16)
project(WorkspaceFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

add_executable(WorkspaceFlow
    src/main.cpp
    src/core/workspace_manager.cpp
    src/storage/json_store.cpp
    src/os/process_launcher.cpp
    src/ui/main_window.cpp
    src/ui/workspace_editor_dialog.cpp
)

target_include_directories(WorkspaceFlow PRIVATE
    src/
    third_party/
)

target_link_libraries(WorkspaceFlow PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
)
```

Key points:
- `CMAKE_AUTOMOC ON` handles Qt's meta-object compilation automatically
- `Qt6::Concurrent` is required for `QtConcurrent::run()` in the async launch path
- No other external dependencies — `nlohmann/json` is header-only in `third_party/`

---

## 9. Testing Strategy

Given this is a small desktop tool, testing focuses on the units that have logic worth validating:

| Unit | What to test | How |
|---|---|---|
| `JsonStore` | Round-trip serialize/deserialize, atomic write (`.tmp` exists then doesn't), missing file returns empty, malformed JSON throws | Unit tests with temp directories (`std::filesystem::temp_directory_path`) |
| `WorkspaceManager` | CRUD operations, launch collects all results even on partial failure | Unit tests with mock/stub `JsonStore` and `ProcessLauncher` (constructor injection makes this trivial) |
| `ProcessLauncher` | Valid app launch, invalid path returns error, URL opens without crash | Manual integration tests on Windows (these call real OS APIs) |
| UI | Buttons enable/disable correctly, editor validates non-empty name | Manual smoke testing. Automated UI testing is out of scope for v1. |

Test framework: A lightweight option like Catch2 (single-header) or Qt Test. The choice is deferred to implementation planning.

---

## 10. Future Considerations (Out of Scope for v1)

These are explicitly **not** part of this design but are noted so the architecture doesn't accidentally prevent them:

- **System tray mode** — minimize to tray, right-click to launch workspaces
- **Keyboard shortcuts / global hotkeys** — launch a workspace from anywhere
- **Import/export** — share workspace configs as `.json` files
- **Workspace ordering** — drag-to-reorder in the list

The current architecture (manager with injected dependencies, UUID-based workspace identity, versioned JSON schema) accommodates all of these without structural changes.
