#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "devflow/config/Config.hpp"

namespace devflow {

// Value-based parse failure. Parsing never throws to the caller.
struct ParseError {
    std::string message;
};

using ParseResult = std::variant<Config, ParseError>;

// Parse a UTF-8 YAML file. Returns Config on success, ParseError otherwise.
[[nodiscard]] ParseResult parse_config_file(std::string_view path);

// Parse a YAML document held in memory (used by tests).
[[nodiscard]] ParseResult parse_config_string(std::string_view yaml);

}  // namespace devflow
