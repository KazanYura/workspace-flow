#pragma once

#include <functional>
#include <string_view>

namespace devflow {

// A logging sink keeps console/UI concerns out of the core engine. The console
// app provides a thread-safe, colorizing implementation; tests inject a fake.
using LogSink = std::function<void(std::string_view)>;

}  // namespace devflow
