#include "devflow/console/ConsoleLog.hpp"

#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>

#include "devflow/console/Console.hpp"

namespace devflow {
namespace {

// Picks a color from a log line's content. Reset means "print unchanged".
Color color_for(std::string_view line) noexcept {
    constexpr auto npos = std::string_view::npos;
    if (line.find("CRITICAL") != npos) return Color::Red;
    if (line.find("WARNING") != npos) return Color::Yellow;
    if (line.find("recovered") != npos) return Color::Green;
    if (line.starts_with("[thermal]")) return Color::Gray;
    if (line.starts_with("  -> succeeded")) return Color::Green;
    if (line.starts_with("  -> failed")) return Color::Red;
    if (line.starts_with("  -> skipped")) return Color::Yellow;
    if (line.starts_with("[")) return Color::Bold;
    return Color::Reset;
}

}  // namespace

LogSink make_console_log_sink() {
    auto mutex = std::make_shared<std::mutex>();
    return [mutex](std::string_view line) {
        const Color color = color_for(line);
        const std::scoped_lock lock(*mutex);
        if (color == Color::Reset) {
            std::cout << line << '\n';
        } else {
            std::cout << colorize(line, color) << '\n';
        }
    };
}

}  // namespace devflow
