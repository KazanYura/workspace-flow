#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace devflow {

// Outcome of running a child process to completion. Value-based: never throws.
struct ProcessResult {
    bool launched = false;        // CreateProcessW succeeded
    bool timed_out = false;       // killed after exceeding the timeout
    unsigned long exit_code = 0;  // meaningful only when launched && !timed_out
    std::string output;           // merged stdout+stderr, raw child bytes
    std::string error;            // failure reason when launched == false

    [[nodiscard]] bool succeeded() const noexcept {
        return launched && !timed_out && exit_code == 0;
    }
};

// Run a command line through cmd.exe /C, capturing merged stdout/stderr and
// waiting for exit. timeout_sec absent or <= 0 => wait indefinitely; otherwise
// the child is terminated once it elapses and timed_out is set.
[[nodiscard]] ProcessResult run_process(std::string_view command_line,
                                        std::optional<int> timeout_sec = std::nullopt);

// Launch a process detached: no capture, no wait, returns once it has started.
// Used for a gate's pre_command (e.g., a VPN GUI). Returns launch success.
[[nodiscard]] bool launch_detached(std::string_view command_line);

}  // namespace devflow
