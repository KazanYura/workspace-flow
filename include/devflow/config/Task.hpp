#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace devflow {

// The pipeline supports exactly three task types: command | poll | gate.

// Launch a process and wait for it to exit (success == exit code 0).
struct CommandTask {
    std::string command;
    std::optional<int> timeout_sec;  // absent => wait indefinitely
};

// Run a readiness probe on a loop until it succeeds or retries are exhausted.
struct PollTask {
    std::string command;
    int retry_interval_sec = 2;
    int max_retries = 10;
};

// Pause the pipeline for explicit user confirmation (e.g., 2FA).
struct GateTask {
    std::string message;
    std::optional<std::string> pre_command;  // optional process to launch first
};

using TaskAction = std::variant<CommandTask, PollTask, GateTask>;

struct Task {
    std::string id;
    std::string name;
    TaskAction action;
};

// Human-readable YAML type key for a task's action.
[[nodiscard]] inline std::string_view task_type_name(const TaskAction& action) noexcept {
    switch (action.index()) {
        case 0: return "command";
        case 1: return "poll";
        case 2: return "gate";
        default: return "unknown";
    }
}

}  // namespace devflow
