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
        } else {
            options.config_path = std::string{arg};
        }
    }
    return options;
}

}  // namespace devflow
