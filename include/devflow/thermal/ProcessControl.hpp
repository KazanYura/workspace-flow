#pragma once

#include <cstddef>
#include <string_view>

namespace devflow {

// Thermal mitigation against processes selected by image name (e.g. "chrome.exe").
// Matching is case-insensitive and affects every matching process. Each function
// returns the number of processes it acted on.

// Suspend all threads of matching processes (reversible throttle).
std::size_t suspend_processes(std::string_view image_name);

// Resume all threads of matching processes.
std::size_t resume_processes(std::string_view image_name);

// Forcibly terminate matching processes (non-reversible).
std::size_t terminate_processes(std::string_view image_name);

}  // namespace devflow
