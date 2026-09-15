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
#include "devflow/pipeline/Subprocess.hpp"
#include "devflow/thermal/ProcessControl.hpp"
#include "devflow/thermal/CompositeTemperatureSensor.hpp"
#include "devflow/thermal/ThermalMonitor.hpp"
#include "devflow/thermal/WmiTemperatureSensor.hpp"
#include "devflow/system/Autostart.hpp"

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
    if (options.help) {
        std::cout << "Usage: devflow [config.yaml] [--dry-run] [--continue-on-error] "
                     "[--log-file path] [--register-autostart|--unregister-autostart]\n";
        return 0;
    }
    if (options.register_autostart || options.unregister_autostart) {
        std::string error;
        const bool enabled = options.register_autostart && !options.unregister_autostart;
        if (!devflow::set_autostart(enabled, error)) {
            std::cerr << "Auto-start update failed: " << error << '\n';
            return 1;
        }
        std::cout << (enabled ? "Auto-start registered.\n" : "Auto-start unregistered.\n");
        return 0;
    }

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

    const devflow::LogSink console_log = devflow::make_console_log_sink();
    const devflow::LogSink file_log = options.log_file
                                          ? devflow::make_file_log_sink(*options.log_file)
                                          : devflow::LogSink{};
    const devflow::LogSink log = [console_log, file_log](std::string_view line) {
        if (console_log) console_log(line);
        if (file_log) file_log(line);
    };

    devflow::ThermalMonitor thermal(config.thermal,
                                    devflow::make_temperature_sensor(),
                                    log,
                                    make_process_mitigation(log));
    thermal.start();

    const int exit_code = devflow::execute_pipeline(config.pipeline, log,
                                                    config.pipeline_timeout_sec,
                                                    config.continue_on_error ||
                                                        options.continue_on_error);

    if (exit_code != 0 && config.on_failure_command) {
        if (!devflow::launch_detached(*config.on_failure_command)) {
            log(std::format("[pipeline] failed to launch on_failure_command: {}",
                            *config.on_failure_command));
        }
    }

    thermal.stop();
    return exit_code;
}
