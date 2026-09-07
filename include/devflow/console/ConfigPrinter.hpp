#pragma once

#include <string_view>

#include "devflow/config/Config.hpp"

namespace devflow {

// Writes a human-readable summary of a parsed config to stdout.
void print_config(std::string_view path, const Config& config);

}  // namespace devflow
