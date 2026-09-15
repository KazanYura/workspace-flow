#include "devflow/pipeline/PipelineRunner.hpp"

#include <chrono>
#include <format>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include "devflow/pipeline/Subprocess.hpp"

namespace devflow {

std::string_view to_string(TaskStatus status) noexcept {
    switch (status) {
        case TaskStatus::Pending:   return "pending";
        case TaskStatus::Running:   return "running";
        case TaskStatus::Succeeded: return "succeeded";
        case TaskStatus::Failed:    return "failed";
        case TaskStatus::Skipped:   return "skipped";
    }
    return "unknown";
}

PipelineRunner::PipelineRunner(LogSink log, GateHandler gate, ProgressObserver progress)
    : log_(std::move(log)), gate_(std::move(gate)), progress_(std::move(progress)) {}

void PipelineRunner::log(std::string_view line) const {
    if (log_) log_(line);
}

TaskOutcome PipelineRunner::run_command(const Task& task, const CommandTask& action,
                                         std::optional<int> timeout_sec) const {
    log(std::format("  running command: {}", action.command));
    ProcessResult res;
    const int attempts = action.max_retries > 0 ? action.max_retries : 1;
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        res = run_process(action.command, timeout_sec ? timeout_sec : action.timeout_sec,
                          action.shell);
        if (res.succeeded() || res.timed_out || attempt == attempts) break;
        log(std::format("  command attempt {}/{} failed", attempt, attempts));
        if (action.retry_interval_sec > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(action.retry_interval_sec));
        }
    }

    if (!res.output.empty()) {
        log(res.output);
    }
    if (!res.launched) {
        return {task.id, TaskStatus::Failed, res.error};
    }
    if (res.timed_out) {
        return {task.id, TaskStatus::Failed,
                std::format("timed out after {}s", action.timeout_sec.value_or(0))};
    }
    if (res.exit_code != 0) {
        return {task.id, TaskStatus::Failed, std::format("exit code {}", res.exit_code)};
    }
    return {task.id, TaskStatus::Succeeded, "exit code 0"};
}

TaskOutcome PipelineRunner::run_poll(const Task& task, const PollTask& action,
                                     std::stop_token stop,
                                     std::optional<int> timeout_sec) const {
    const int max_retries = action.max_retries > 0 ? action.max_retries : 1;
    const int interval = action.retry_interval_sec > 0 ? action.retry_interval_sec : 0;

    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        if (stop.stop_requested()) {
            return {task.id, TaskStatus::Failed, "stopped"};
        }
        log(std::format("  poll attempt {}/{}: {}", attempt, max_retries, action.command));
        const ProcessResult res = run_process(action.command, timeout_sec, action.shell);
        if (res.succeeded()) {
            return {task.id, TaskStatus::Succeeded,
                    std::format("ready after {} attempt(s)", attempt)};
        }
        if (attempt < max_retries) {
            // Sleep a second at a time so a stop request interrupts the wait.
            for (int elapsed = 0; elapsed < interval && !stop.stop_requested(); ++elapsed) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    return {task.id, TaskStatus::Failed,
            std::format("not ready after {} attempt(s)", max_retries)};
}

TaskOutcome PipelineRunner::run_gate(const Task& task, const GateTask& action) const {
    if (action.pre_command) {
        const bool launched = launch_detached(*action.pre_command);
        log(launched ? std::format("  launched: {}", *action.pre_command)
                     : std::format("  WARNING: failed to launch {}", *action.pre_command));
    }
    if (!gate_) {
        return {task.id, TaskStatus::Failed, "no gate handler configured"};
    }
    return gate_(action) ? TaskOutcome{task.id, TaskStatus::Succeeded, "confirmed"}
                         : TaskOutcome{task.id, TaskStatus::Failed, "aborted by user"};
}

PipelineResult PipelineRunner::run(const std::vector<Task>& pipeline, std::stop_token stop,
                                   std::optional<int> timeout_sec, bool continue_on_error) {
    PipelineResult result;
    result.outcomes.reserve(pipeline.size());

    bool aborted = false;
    bool failed = false;
    std::vector<const CommandTask*> completed_commands;
    const auto deadline = timeout_sec && *timeout_sec > 0
                              ? std::optional{std::chrono::steady_clock::now() +
                                              std::chrono::seconds(*timeout_sec)}
                              : std::nullopt;
    for (const Task& task : pipeline) {
        if (aborted || stop.stop_requested()) {
            TaskOutcome skipped{task.id, TaskStatus::Skipped,
                                aborted ? "prior task failed" : "pipeline stopped"};
            if (progress_) progress_(skipped);
            result.outcomes.push_back(std::move(skipped));
            aborted = true;  // once stopped, skip the remaining tasks too
            continue;
        }

        log(std::format("[{}] {} ({})", task.id, task.name, task_type_name(task.action)));
        if (progress_) progress_({task.id, TaskStatus::Running, ""});

        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            TaskOutcome timed_out{task.id, TaskStatus::Failed, "pipeline timeout exceeded"};
            if (progress_) progress_(timed_out);
            result.outcomes.push_back(std::move(timed_out));
            aborted = true;
            failed = true;
            continue;
        }
        const auto task_started = std::chrono::steady_clock::now();
        const auto remaining_timeout = [&]() -> std::optional<int> {
            if (!deadline) return std::nullopt;
            const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                *deadline - std::chrono::steady_clock::now()).count();
            return static_cast<int>(remaining > 0 ? remaining : 1);
        };
        TaskOutcome outcome = std::visit([&](const auto& action) -> TaskOutcome {
            using T = std::decay_t<decltype(action)>;
            if constexpr (std::is_same_v<T, CommandTask>) {
                return run_command(task, action, remaining_timeout());
            } else if constexpr (std::is_same_v<T, PollTask>) {
                return run_poll(task, action, stop, remaining_timeout());
            } else {
                return run_gate(task, action);  // GateTask
            }
        }, task.action);
        outcome.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - task_started).count();

        log(std::format("  -> {}: {} ({} ms)", to_string(outcome.status), outcome.detail,
                        outcome.duration_ms));
        if (progress_) progress_(outcome);

        if (outcome.status == TaskStatus::Succeeded) {
            if (const auto* command = std::get_if<CommandTask>(&task.action);
                command && command->undo_command) {
                completed_commands.push_back(command);
            }
        }

        if (outcome.status == TaskStatus::Failed) {
            failed = true;
            if (!continue_on_error) aborted = true;
        }
        result.outcomes.push_back(std::move(outcome));
    }

    if (failed) {
        for (auto it = completed_commands.rbegin(); it != completed_commands.rend(); ++it) {
            log(std::format("  rollback: {}", *(*it)->undo_command));
            const ProcessResult rollback = run_process(*(*it)->undo_command);
            if (!rollback.output.empty()) log(rollback.output);
            if (!rollback.succeeded()) {
                log(std::format("  rollback failed: {}", rollback.error.empty()
                                                           ? std::format("exit code {}", rollback.exit_code)
                                                           : rollback.error));
            }
        }
    }

    result.ok = !failed && !aborted;
    return result;
}

}  // namespace devflow
