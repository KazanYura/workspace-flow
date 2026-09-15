#pragma once

#include <optional>
#include <string>

namespace devflow {

// Parsed command-line options for the console application.
struct CommandLineOptions {
    std::string config_path = "docs/example_config.yaml";
    bool dry_run = false;  // parse and print only, never launch processes
    bool help = false;
    bool continue_on_error = false;
    bool register_autostart = false;
    bool unregister_autostart = false;
    std::optional<std::string> log_file;
};

[[nodiscard]] CommandLineOptions parse_arguments(int argc, char** argv);

}  // namespace devflow
