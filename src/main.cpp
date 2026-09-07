#include <cstddef>
#include <format>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "devflow/console/CommandLine.hpp"
#include "devflow/config/ConfigParser.hpp"
#include "devflow/console/ConfigPrinter.hpp"
#include "devflow/console/Console.hpp"
#include "devflow/console/ConsoleLog.hpp"
#include "devflow/console/ConsoleRunner.hpp"
#include "devflow/thermal/ProcessControl.hpp"
#include "devflow/thermal/ThermalMonitor.hpp"
#include "devflow/thermal/WmiTemperatureSensor.hpp"

namespace {

// Translates the monitor's throttle/restore decisions into real Win32 actions:
// suspend the target processes on a critical breach, resume them on recovery.
devflow::MitigationHandler make_process_mitigation(const devflow::LogSink& log) {
    return [log](devflow::MitigationAction action, const std::vector<std::string>& targets) {
        const bool throttle = action == devflow::MitigationAction::Throttle;
        for (const auto& name : targets) {
            const std::size_t count =
                throttle ? devflow::suspend_processes(name) : devflow::resume_processes(name);
            if (log) {
                log(std::format("[thermal] {} {} process(es) named '{}'",
                                throttle ? "suspended" : "resumed", count, name));
            }
        }
    };
}

}  // namespace

int main(int argc, char** argv) {
    devflow::enable_colors();
    const devflow::CommandLineOptions options = devflow::parse_arguments(argc, argv);

    const auto parse_result = devflow::parse_config_file(options.config_path);
    if (const auto* error = std::get_if<devflow::ParseError>(&parse_result)) {
        std::cerr << devflow::colorize(
                         std::format("Failed to parse '{}': {}", options.config_path, error->message),
                         devflow::Color::Red)
                  << '\n';
        return 1;
    }

    const auto& config = std::get<devflow::Config>(parse_result);
    devflow::print_config(options.config_path, config);

    if (options.dry_run) {
        return 0;
    }

    const devflow::LogSink log = devflow::make_console_log_sink();

    devflow::ThermalMonitor thermal(config.thermal,
                                    devflow::make_wmi_sensor(),
                                    log,
                                    make_process_mitigation(log));
    thermal.start();

    const int exit_code = devflow::execute_pipeline(config.pipeline, log);

    thermal.stop();
    return exit_code;
}
