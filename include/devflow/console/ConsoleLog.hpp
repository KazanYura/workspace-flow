#pragma once

#include <optional>
#include <string>

#include "devflow/common/Logging.hpp"

namespace devflow {

// Returns a thread-safe LogSink that writes colorized lines to stdout. Both the
// PipelineRunner (main thread) and ThermalMonitor (telemetry thread) share one
// instance so their output never interleaves mid-line.
[[nodiscard]] LogSink make_console_log_sink();
[[nodiscard]] LogSink make_file_log_sink(const std::string& path);

}  // namespace devflow
