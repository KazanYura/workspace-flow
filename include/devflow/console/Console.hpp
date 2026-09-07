#pragma once

#include <string>
#include <string_view>

namespace devflow {

// ANSI SGR colors used for console output.
enum class Color { Reset, Bold, Red, Green, Yellow, Blue, Magenta, Cyan, Gray };

// Enables ANSI escape processing when stdout is an interactive terminal and
// returns true when colored output is active. Call once at startup; if output
// is redirected to a file or pipe, colors stay disabled.
bool enable_colors();

// Wraps `text` in the given color plus a trailing reset. Returns `text`
// unchanged when colors are disabled.
[[nodiscard]] std::string colorize(std::string_view text, Color color);

}  // namespace devflow
