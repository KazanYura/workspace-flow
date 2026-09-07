#include "devflow/config/ConfigParser.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace devflow {
namespace {

ParseError make_error(std::string message) {
    return ParseError{std::move(message)};
}

// Read an entire file as raw UTF-8 bytes.
std::optional<std::string> read_file(std::string_view path) {
    std::ifstream in(std::string{path}, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Parse the optional `thermal_monitor` section. Absent => defaults.
std::optional<ThermalConfig> parse_thermal(const YAML::Node& node, std::string& error) {
    ThermalConfig cfg;
    if (!node) {
        return cfg;
    }
    if (!node.IsMap()) {
        error = "'thermal_monitor' must be a map";
        return std::nullopt;
    }
    if (node["enabled"]) cfg.enabled = node["enabled"].as<bool>();
    if (node["poll_interval_sec"]) cfg.poll_interval_sec = node["poll_interval_sec"].as<int>();
    if (node["warning_temp_c"]) cfg.warning_temp_c = node["warning_temp_c"].as<int>();
    if (node["critical_temp_c"]) cfg.critical_temp_c = node["critical_temp_c"].as<int>();
    if (const auto targets = node["throttle_targets"]) {
        for (const auto& item : targets) {
            cfg.throttle_targets.push_back(item.as<std::string>());
        }
    }
    return cfg;
}

// Parse a single pipeline entry into a Task. Sets `error` and returns nullopt on failure.
std::optional<Task> parse_task(const YAML::Node& node, std::string& error) {
    if (!node.IsMap()) {
        error = "each 'pipeline' entry must be a map";
        return std::nullopt;
    }

    Task task;
    if (!node["id"]) {
        error = "a task is missing required field 'id'";
        return std::nullopt;
    }
    task.id = node["id"].as<std::string>();
    task.name = node["name"] ? node["name"].as<std::string>() : task.id;

    if (!node["type"]) {
        error = "task '" + task.id + "' is missing required field 'type'";
        return std::nullopt;
    }
    const auto type = node["type"].as<std::string>();

    if (type == "command") {
        CommandTask action;
        if (!node["command"]) {
            error = "command task '" + task.id + "' is missing 'command'";
            return std::nullopt;
        }
        action.command = node["command"].as<std::string>();
        if (node["timeout_sec"]) action.timeout_sec = node["timeout_sec"].as<int>();
        task.action = std::move(action);
    } else if (type == "poll") {
        PollTask action;
        if (!node["command"]) {
            error = "poll task '" + task.id + "' is missing 'command'";
            return std::nullopt;
        }
        action.command = node["command"].as<std::string>();
        if (node["retry_interval_sec"]) action.retry_interval_sec = node["retry_interval_sec"].as<int>();
        if (node["max_retries"]) action.max_retries = node["max_retries"].as<int>();
        task.action = std::move(action);
    } else if (type == "gate") {
        GateTask action;
        if (!node["message"]) {
            error = "gate task '" + task.id + "' is missing 'message'";
            return std::nullopt;
        }
        action.message = node["message"].as<std::string>();
        if (node["pre_command"]) action.pre_command = node["pre_command"].as<std::string>();
        task.action = std::move(action);
    } else {
        error = "task '" + task.id + "' has unknown type '" + type +
                "' (expected command | poll | gate)";
        return std::nullopt;
    }

    return task;
}

}  // namespace

ParseResult parse_config_string(std::string_view yaml) {
    try {
        const YAML::Node root = YAML::Load(std::string{yaml});
        if (!root || !root.IsMap()) {
            return make_error("root document must be a YAML map");
        }

        Config config;
        std::string sub_error;

        auto thermal = parse_thermal(root["thermal_monitor"], sub_error);
        if (!thermal) {
            return make_error(std::move(sub_error));
        }
        config.thermal = std::move(*thermal);

        const auto pipeline = root["pipeline"];
        if (!pipeline || !pipeline.IsSequence()) {
            return make_error("'pipeline' must be a sequence of tasks");
        }
        for (const auto& item : pipeline) {
            auto task = parse_task(item, sub_error);
            if (!task) {
                return make_error(std::move(sub_error));
            }
            config.pipeline.push_back(std::move(*task));
        }

        return config;
    } catch (const YAML::Exception& e) {
        // Convert library exceptions into a value at the boundary.
        return make_error(std::string{"YAML parse error: "} + e.what());
    }
}

ParseResult parse_config_file(std::string_view path) {
    auto content = read_file(path);
    if (!content) {
        return make_error("cannot open config file: " + std::string{path});
    }
    return parse_config_string(*content);
}

}  // namespace devflow
