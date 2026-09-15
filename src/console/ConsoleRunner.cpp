#include "devflow/console/ConsoleRunner.hpp"

#include <format>
#include <iostream>
#include <string>

#include "devflow/console/Console.hpp"
#include "devflow/pipeline/PipelineRunner.hpp"

namespace devflow {
namespace {

// Returns true to continue past the gate, false to abort the pipeline.
bool prompt_at_gate(const GateTask& gate) {
    std::cout << colorize(
        std::format("\n>>> GATE: {}\n>>> Press Enter to continue (or type 'abort'): ",
                    gate.message),
        Color::Yellow);
    std::string response;
    std::getline(std::cin, response);
    return response != "abort";
}

}  // namespace

int execute_pipeline(const std::vector<Task>& pipeline, const LogSink& log,
                     std::optional<int> timeout_sec, bool continue_on_error) {
    std::cout << '\n' << colorize("--- Executing pipeline ---", Color::Bold) << '\n';
    PipelineRunner runner(log, prompt_at_gate);

    const auto result = runner.run(pipeline, {}, timeout_sec, continue_on_error);
    const std::string_view verdict = result.ok ? "SUCCEEDED" : "FAILED";
    std::cout << "\nPipeline "
              << colorize(verdict, result.ok ? Color::Green : Color::Red) << ".\n";
    return result.ok ? 0 : 1;
}

}  // namespace devflow
