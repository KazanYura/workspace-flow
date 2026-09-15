#pragma once

#include <optional>
#include <string>
#include <vector>

#include "devflow/config/Task.hpp"
#include "devflow/config/ThermalConfig.hpp"

namespace devflow {

// Top-level parsed representation of a DevFlow config file.
struct Config {
    ThermalConfig thermal;
    std::vector<Task> pipeline;  // executed strictly top-to-bottom
    std::optional<std::string> on_failure_command;  // launched (detached) if the pipeline fails
    std::optional<int> pipeline_timeout_sec;         // overall wall-clock budget
    bool continue_on_error = false;

    // Returns human-readable problems with this config; empty means valid.
    // Checked in addition to the parser's own required-field checks.
    [[nodiscard]] std::vector<std::string> validate() const;
};

}  // namespace devflow
