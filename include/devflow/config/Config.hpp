#pragma once

#include <vector>

#include "devflow/config/Task.hpp"
#include "devflow/config/ThermalConfig.hpp"

namespace devflow {

// Top-level parsed representation of a DevFlow config file.
struct Config {
    ThermalConfig thermal;
    std::vector<Task> pipeline;  // executed strictly top-to-bottom
};

}  // namespace devflow
