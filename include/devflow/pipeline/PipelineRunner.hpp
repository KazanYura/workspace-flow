#pragma once

#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "devflow/common/Logging.hpp"
#include "devflow/config/Task.hpp"

namespace devflow {

enum class TaskStatus { Pending, Running, Succeeded, Failed, Skipped };

[[nodiscard]] std::string_view to_string(TaskStatus status) noexcept;

struct TaskOutcome {
    std::string id;
    TaskStatus status = TaskStatus::Pending;
    std::string detail;  // human-readable reason
};

struct PipelineResult {
    std::vector<TaskOutcome> outcomes;
    bool ok = false;  // true iff no task failed
};

// Returns true to proceed past a gate, false to abort the pipeline.
using GateHandler = std::function<bool(const GateTask&)>;

// Reports each task's transition to Running and then its terminal outcome as it
// happens, so a UI can reflect live progress. Optional; console mode omits it.
using ProgressObserver = std::function<void(const TaskOutcome&)>;

// Executes a parsed pipeline top-to-bottom. Fail-fast: the first failing task
// skips the remainder. Synchronous for the console MVP (Phase 2); a UI runs it
// on its own thread and can cancel it via the stop_token passed to run().
class PipelineRunner {
public:
    PipelineRunner(LogSink log, GateHandler gate, ProgressObserver progress = {});

    [[nodiscard]] PipelineResult run(const std::vector<Task>& pipeline,
                                     std::stop_token stop = {});

private:
    void log(std::string_view line) const;
    [[nodiscard]] TaskOutcome run_command(const Task& task, const CommandTask& action) const;
    [[nodiscard]] TaskOutcome run_poll(const Task& task, const PollTask& action,
                                       std::stop_token stop) const;
    [[nodiscard]] TaskOutcome run_gate(const Task& task, const GateTask& action) const;

    LogSink log_;
    GateHandler gate_;
    ProgressObserver progress_;
};

}  // namespace devflow
