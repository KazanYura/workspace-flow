#include "devflow/console/Console.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace devflow {
namespace {

bool g_colors_enabled = false;

constexpr std::string_view sgr_code(Color color) noexcept {
    switch (color) {
        case Color::Reset:   return "\x1b[0m";
        case Color::Bold:    return "\x1b[1m";
        case Color::Red:     return "\x1b[31m";
        case Color::Green:   return "\x1b[32m";
        case Color::Yellow:  return "\x1b[33m";
        case Color::Blue:    return "\x1b[34m";
        case Color::Magenta: return "\x1b[35m";
        case Color::Cyan:    return "\x1b[36m";
        case Color::Gray:    return "\x1b[90m";
    }
    return {};
}

bool stdout_is_terminal() noexcept {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

#ifdef _WIN32
// GetStdHandle returns a non-owning handle owned by the OS; never close it.
bool enable_virtual_terminal(DWORD which_stream) noexcept {
    const HANDLE stream = GetStdHandle(which_stream);
    if (stream == nullptr || stream == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    if (GetConsoleMode(stream, &mode) == 0) {
        return false;
    }
    return SetConsoleMode(stream, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}
#endif

}  // namespace

bool enable_colors() {
    if (!stdout_is_terminal()) {
        g_colors_enabled = false;
        return false;
    }
#ifdef _WIN32
    const bool ok = enable_virtual_terminal(STD_OUTPUT_HANDLE);
    enable_virtual_terminal(STD_ERROR_HANDLE);
    g_colors_enabled = ok;
#else
    g_colors_enabled = true;
#endif
    return g_colors_enabled;
}

std::string colorize(std::string_view text, Color color) {
    if (!g_colors_enabled) {
        return std::string{text};
    }
    const std::string_view code = sgr_code(color);
    const std::string_view reset = sgr_code(Color::Reset);
    std::string out;
    out.reserve(code.size() + text.size() + reset.size());
    out.append(code).append(text).append(reset);
    return out;
}

}  // namespace devflow
