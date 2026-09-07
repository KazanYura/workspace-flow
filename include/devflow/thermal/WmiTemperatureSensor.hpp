#pragma once

#include <memory>

#include "devflow/thermal/TemperatureSensor.hpp"

namespace devflow {

// Creates a sensor backed by WMI's MSAcpi_ThermalZoneTemperature (root\WMI).
// Each read initializes COM on the calling thread, so the returned sensor is
// safe to use from ThermalMonitor's telemetry thread.
[[nodiscard]] std::unique_ptr<ITemperatureSensor> make_wmi_sensor();

}  // namespace devflow
