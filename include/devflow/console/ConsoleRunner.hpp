#pragma once

#include <optional>
#include <vector>

#include "devflow/common/Logging.hpp"
#include "devflow/config/Task.hpp"

namespace devflow {

// Runs the pipeline with the given log sink and interactive gate prompts.
// Returns a process exit code (0 on success, 1 on failure).
[[nodiscard]] int execute_pipeline(const std::vector<Task>& pipeline, const LogSink& log,
								   std::optional<int> timeout_sec = std::nullopt,
								   bool continue_on_error = false);

}  // namespace devflow
