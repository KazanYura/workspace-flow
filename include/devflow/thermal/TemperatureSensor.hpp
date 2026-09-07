#pragma once

#include <optional>

namespace devflow {

// Sensor abstraction so temperature sources (WMI, PDH, a fake in tests) can be
// swapped without touching ThermalMonitor.
class ITemperatureSensor {
public:
    virtual ~ITemperatureSensor() = default;

    // Current temperature in Celsius, or nullopt when unavailable this tick
    // (e.g., hardware exposes no ACPI thermal zone).
    [[nodiscard]] virtual std::optional<double> ReadCelsius() = 0;
};

}  // namespace devflow
