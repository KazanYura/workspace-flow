#pragma once

#include <string>

namespace devflow {

[[nodiscard]] bool set_autostart(bool enabled, std::string& error);

}