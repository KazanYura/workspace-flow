#include "devflow/config/Config.hpp"

#include <format>
#include <type_traits>
#include <unordered_set>
#include <variant>

namespace devflow {

std::vector<std::string> Config::validate() const {
    std::vector<std::string> errors;

    if (thermal.enabled) {
        if (thermal.poll_interval_sec <= 0) {
            errors.push_back("thermal_monitor.poll_interval_sec must be > 0");
        }
        if (thermal.warning_temp_c >= thermal.critical_temp_c) {
            errors.push_back("thermal_monitor.warning_temp_c must be less than critical_temp_c");
        }
        if (thermal.critical_temp_c > 150) {
            errors.push_back("thermal_monitor.critical_temp_c is implausibly high (> 150C)");
        }
        for (const auto& target : thermal.throttle_targets) {
            if (target.empty()) {
                errors.push_back("thermal_monitor.throttle_targets entries must not be empty");
                break;
            }
        }
    }

    if (pipeline.empty()) {
        errors.push_back("'pipeline' must contain at least one task");
    }

    if (pipeline_timeout_sec && *pipeline_timeout_sec <= 0) {
        errors.push_back("pipeline_timeout_sec must be > 0 when present");
    }

    std::unordered_set<std::string> seen_ids;
    for (const Task& task : pipeline) {
        if (!seen_ids.insert(task.id).second) {
            errors.push_back(std::format("duplicate task id '{}'", task.id));
        }

        std::visit([&](const auto& action) {
            using T = std::decay_t<decltype(action)>;
            if constexpr (std::is_same_v<T, CommandTask>) {
                if (action.command.empty()) {
                    errors.push_back(std::format("task '{}' has an empty command", task.id));
                }
                if (action.undo_command && action.undo_command->empty()) {
                    errors.push_back(std::format("task '{}' has an empty undo_command", task.id));
                }
                if (action.max_retries <= 0) {
                    errors.push_back(std::format("task '{}' max_retries must be > 0", task.id));
                }
                if (action.retry_interval_sec < 0) {
                    errors.push_back(std::format("task '{}' retry_interval_sec must be >= 0", task.id));
                }
            } else if constexpr (std::is_same_v<T, PollTask>) {
                if (action.command.empty()) {
                    errors.push_back(std::format("task '{}' has an empty command", task.id));
                }
                if (action.max_retries <= 0) {
                    errors.push_back(std::format("task '{}' max_retries must be > 0", task.id));
                }
                if (action.retry_interval_sec < 0) {
                    errors.push_back(std::format("task '{}' retry_interval_sec must be >= 0", task.id));
                }
            } else {
                if (action.message.empty()) {
                    errors.push_back(std::format("task '{}' has an empty gate message", task.id));
                }
            }
        }, task.action);
    }

    return errors;
}

}  // namespace devflow
