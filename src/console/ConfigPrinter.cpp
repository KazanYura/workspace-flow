#include "devflow/console/ConfigPrinter.hpp"

#include <cstddef>
#include <format>
#include <iostream>
#include <type_traits>
#include <variant>
#include <vector>

#include "devflow/console/Console.hpp"
#include "devflow/config/Task.hpp"
#include "devflow/config/ThermalConfig.hpp"

namespace devflow {
namespace {

void print_action_details(const TaskAction& action) {
    std::visit([](const auto& details) {
        using T = std::decay_t<decltype(details)>;
        if constexpr (std::is_same_v<T, CommandTask>) {
            std::cout << std::format("        command: {}\n", colorize(details.command, Color::Gray));
            if (details.timeout_sec) {
                std::cout << std::format("        timeout_sec: {}\n", *details.timeout_sec);
            }
        } else if constexpr (std::is_same_v<T, PollTask>) {
            std::cout << std::format("        command: {}\n", colorize(details.command, Color::Gray));
            std::cout << std::format("        retry_interval_sec: {}\n", details.retry_interval_sec);
            std::cout << std::format("        max_retries: {}\n", details.max_retries);
        } else if constexpr (std::is_same_v<T, GateTask>) {
            std::cout << std::format("        message: {}\n", details.message);
            if (details.pre_command) {
                std::cout << std::format("        pre_command: {}\n", colorize(*details.pre_command, Color::Gray));
            }
        }
    }, action);
}

void print_task(std::size_t index, const Task& task) {
    std::cout << std::format("  [{}] {} - \"{}\"  (type={})\n",
                             index,
                             colorize(task.id, Color::Cyan),
                             task.name,
                             colorize(task_type_name(task.action), Color::Yellow));
    print_action_details(task.action);
}

void print_thermal_config(const ThermalConfig& thermal) {
    std::cout << colorize("Thermal monitor:", Color::Bold) << '\n';
    std::cout << std::format("  enabled: {}\n",
                             colorize(thermal.enabled ? "true" : "false",
                                      thermal.enabled ? Color::Green : Color::Red));
    std::cout << std::format("  poll_interval_sec: {}\n", thermal.poll_interval_sec);
    std::cout << std::format("  warning_temp_c: {}\n", thermal.warning_temp_c);
    std::cout << std::format("  critical_temp_c: {}\n", thermal.critical_temp_c);
    std::cout << "  throttle_targets:";
    for (const auto& target : thermal.throttle_targets) {
        std::cout << ' ' << colorize(target, Color::Gray);
    }
    std::cout << "\n\n";
}

void print_pipeline(const std::vector<Task>& pipeline) {
    std::cout << colorize(std::format("Pipeline ({} task(s)):", pipeline.size()), Color::Bold) << '\n';
    for (std::size_t i = 0; i < pipeline.size(); ++i) {
        print_task(i + 1, pipeline[i]);
    }
}

}  // namespace

void print_config(std::string_view path, const Config& config) {
    std::cout << colorize(std::format("DevFlow Orchestrator - parsed config: {}", path), Color::Bold)
              << "\n\n";
    print_thermal_config(config.thermal);
    print_pipeline(config.pipeline);
}

}  // namespace devflow
