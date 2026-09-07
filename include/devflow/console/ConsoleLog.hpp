#pragma once

#include "devflow/common/Logging.hpp"

namespace devflow {

// Returns a thread-safe LogSink that writes colorized lines to stdout. Both the
// PipelineRunner (main thread) and ThermalMonitor (telemetry thread) share one
// instance so their output never interleaves mid-line.
[[nodiscard]] LogSink make_console_log_sink();

}  // namespace devflow
