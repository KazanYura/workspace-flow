#include "devflow/console/CommandLine.hpp"

#include <string>
#include <string_view>

namespace devflow {

CommandLineOptions parse_arguments(int argc, char** argv) {
    CommandLineOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--dry-run") {
            options.dry_run = true;
        } else if (arg == "--continue-on-error") {
            options.continue_on_error = true;
        } else if (arg == "--register-autostart") {
            options.register_autostart = true;
        } else if (arg == "--unregister-autostart") {
            options.unregister_autostart = true;
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (arg == "--log-file" && i + 1 < argc) {
            options.log_file = argv[++i];
        } else {
            options.config_path = std::string{arg};
        }
    }
    return options;
}

}  // namespace devflow
