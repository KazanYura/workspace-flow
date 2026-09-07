#pragma once

#include <string>
#include <vector>

namespace devflow {

// Settings for the background thermal watchdog (see ROADMAP Phase 3).
struct ThermalConfig {
    bool enabled = true;
    int poll_interval_sec = 5;
    int warning_temp_c = 80;
    int critical_temp_c = 90;
    // Process image names to suspend/terminate on a critical breach.
    std::vector<std::string> throttle_targets;
};

}  // namespace devflow
