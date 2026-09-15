#pragma once

#include <memory>

#include "devflow/thermal/TemperatureSensor.hpp"

namespace devflow {

[[nodiscard]] std::unique_ptr<ITemperatureSensor> make_temperature_sensor();

}