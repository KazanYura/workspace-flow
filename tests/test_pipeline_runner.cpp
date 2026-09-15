// Phase 2: exercises the PipelineRunner state machine with injected sinks.
// Uses cmd.exe built-ins so no external tools are required.

#include <cstdio>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "devflow/pipeline/PipelineRunner.hpp"
#include "devflow/config/Task.hpp"

using namespace devflow;

namespace {

int g_failures = 0;

void check(bool condition, std::string_view what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %.*s\n", static_cast<int>(what.size()), what.data());
        ++g_failures;
    }
}

Task command_task(std::string id, std::string command) {
    Task t;
    t.id = id;
    t.name = std::move(id);
    t.action = CommandTask{std::move(command), std::nullopt};
    return t;
}

Task poll_task(std::string id, std::string command, int interval, int retries) {
    Task t;
    t.id = id;
    t.name = std::move(id);
    t.action = PollTask{std::move(command), interval, retries};
    return t;
}

Task gate_task(std::string id, std::string message) {
    Task t;
    t.id = id;
    t.name = std::move(id);
    t.action = GateTask{std::move(message), std::nullopt};
    return t;
}

}  // namespace

int main() {
    const LogSink silent = [](std::string_view) {};
    const GateHandler auto_confirm = [](const GateTask&) { return true; };
    const GateHandler always_abort = [](const GateTask&) { return false; };

    // Happy path: command + poll + gate all succeed.
    {
        std::vector<Task> pipeline;
        pipeline.push_back(command_task("c1", "exit 0"));
        pipeline.push_back(poll_task("p1", "exit 0", 1, 3));
        pipeline.push_back(gate_task("g1", "confirm?"));

        PipelineRunner runner(silent, auto_confirm);
        const PipelineResult res = runner.run(pipeline);
        check(res.ok, "happy pipeline ok");
        check(res.outcomes.size() == 3, "three outcomes");
        check(res.outcomes[0].status == TaskStatus::Succeeded, "command succeeded");
        check(res.outcomes[1].status == TaskStatus::Succeeded, "poll succeeded");
        check(res.outcomes[2].status == TaskStatus::Succeeded, "gate succeeded");
    }

    // Fail-fast: a failing command skips everything after it.
    {
        std::vector<Task> pipeline;
        pipeline.push_back(command_task("c1", "exit 3"));
        pipeline.push_back(command_task("c2", "exit 0"));

        PipelineRunner runner(silent, auto_confirm);
        const PipelineResult res = runner.run(pipeline);
        check(!res.ok, "failing pipeline not ok");
        check(res.outcomes[0].status == TaskStatus::Failed, "first task failed");
        check(res.outcomes[1].status == TaskStatus::Skipped, "second task skipped");
    }

    // Poll that never succeeds fails after max_retries (interval 0 keeps it fast).
    {
        std::vector<Task> pipeline;
        pipeline.push_back(poll_task("p", "exit 1", 0, 2));

        PipelineRunner runner(silent, auto_confirm);
        const PipelineResult res = runner.run(pipeline);
        check(!res.ok, "failing poll not ok");
        check(res.outcomes[0].status == TaskStatus::Failed, "poll failed");
    }

    // Gate abort fails the pipeline.
    {
        std::vector<Task> pipeline;
        pipeline.push_back(gate_task("g", "confirm?"));

        PipelineRunner runner(silent, always_abort);
        const PipelineResult res = runner.run(pipeline);
        check(!res.ok, "aborted gate not ok");
        check(res.outcomes[0].status == TaskStatus::Failed, "gate failed on abort");
    }

    // Continue-on-error records the failure and still executes later tasks.
    {
        std::vector<Task> pipeline;
        pipeline.push_back(command_task("failed", "exit 3"));
        pipeline.push_back(command_task("after", "exit 0"));

        PipelineRunner runner(silent, auto_confirm);
        const PipelineResult res = runner.run(pipeline, {}, std::nullopt, true);
        check(!res.ok, "continue-on-error remains unsuccessful");
        check(res.outcomes.size() == 2, "continue-on-error runs later task");
        check(res.outcomes[1].status == TaskStatus::Succeeded,
              "continue-on-error later task succeeds");
    }

    // Successful command undo actions run in reverse order after a later failure.
    {
        std::vector<std::string> lines;
        const LogSink capture = [&lines](std::string_view line) {
            lines.emplace_back(line);
        };
        Task first = command_task("first", "exit 0");
        std::get<CommandTask>(first.action).undo_command = "echo undo-first";
        std::vector<Task> pipeline{first, command_task("failed", "exit 4")};

        PipelineRunner runner(capture, auto_confirm);
        const PipelineResult res = runner.run(pipeline);
        check(!res.ok, "rollback pipeline remains failed");
        check(std::any_of(lines.begin(), lines.end(), [](const std::string& line) {
                  return line.find("rollback: echo undo-first") != std::string::npos;
              }), "rollback command executed");
    }

    if (g_failures == 0) {
        std::puts("all tests passed");
        return 0;
    }
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
}
