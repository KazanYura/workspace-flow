#pragma once

#include <string>

namespace devflow {

// Parsed command-line options for the console application.
struct CommandLineOptions {
    std::string config_path = "docs/example_config.yaml";
    bool dry_run = false;  // parse and print only, never launch processes
};

[[nodiscard]] CommandLineOptions parse_arguments(int argc, char** argv);

}  // namespace devflow
