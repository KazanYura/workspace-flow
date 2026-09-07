#pragma once

#include <string>
#include <vector>

namespace devflow::gui {

// Lifecycle state of a single pipeline task, as shown in the dashboard. This is
// a UI-only projection; the runner's internal TaskStatus is mapped onto it so
// the GUI never depends on PipelineRunner internals.
enum class TaskState { Pending, Running, Done, Failed, Skipped };

struct TaskRow {
    std::string name;
    std::string type;  // "command" | "poll" | "gate"
    TaskState state = TaskState::Pending;
};

// Classification of the current thermal reading (mirrors the monitor's bands).
enum class ThermalState { Unknown, Normal, Warning, Critical };

// Plain snapshot the dashboard renders each frame. It is refreshed from the
// orchestrator/thermal threads by GuiPipelineController; the rendering code
// stays agnostic about where the values come from and only raises request_*
// flags back in response to button clicks.
struct DashboardState {
    // Loaded pipeline file + last status/error line shown in the toolbar.
    std::string config_path;
    std::string status_message;
    bool has_config = false;
    bool running = false;

    // Buttons set these; the controller consumes and clears them each frame.
    bool request_open = false;
    bool request_run = false;
    bool request_stop = false;

    std::vector<TaskRow> tasks;
    std::string logs;  // accumulated subprocess stdout/stderr

    float temperature_c = 0.0f;
    ThermalState thermal = ThermalState::Unknown;

    // Interactive gate (e.g. Cisco AnyConnect 2FA). While active a modal blocks
    // the dashboard until the user clicks Continue, which sets the request flag.
    bool gate_active = false;
    std::string gate_title;
    std::string gate_message;
    bool gate_continue_requested = false;

    bool pipeline_finished = false;
};

}  // namespace devflow::gui
