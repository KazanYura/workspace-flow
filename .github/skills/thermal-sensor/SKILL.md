---
name: thermal-sensor
description: 'Read CPU/system temperature on Windows for the DevFlow Orchestrator ThermalMonitor. Use when implementing or debugging the sensor layer — WMI MSAcpi_ThermalZoneTemperature, PDH thermal counters, rolling averages, thresholds, and a fallback for hardware where WMI thermal zones are unavailable.'
---

# Thermal Sensor (WMI / PDH)

Guidance for the temperature-reading layer behind `ThermalMonitor`. Put sensor access behind an interface so implementations can be swapped and unit-tested.

## When to Use
- Implementing `ThermalMonitor`'s sensor reads.
- WMI returns nothing on the target hardware and you need a fallback.
- Tuning polling, rolling average, or warning/critical thresholds.

## Sensor Interface First
```cpp
class ITemperatureSensor {
public:
  virtual ~ITemperatureSensor() = default;
  virtual std::optional<double> ReadCelsius() = 0; // nullopt if unavailable this tick
};
```
`ThermalMonitor` depends on `ITemperatureSensor`, not a concrete API — enables a fake sensor in tests.

## Option A — WMI (`MSAcpi_ThermalZoneTemperature`)
- Namespace `root\WMI`, class `MSAcpi_ThermalZoneTemperature`, property `CurrentTemperature`.
- Value is in **tenths of a Kelvin**: `celsius = tenthsKelvin / 10.0 - 273.15`.
- Steps: `CoInitializeEx` → `CoInitializeSecurity` → `CoCreateInstance(CLSID_WbemLocator)` → `ConnectServer(L"ROOT\\WMI")` → `ExecQuery(L"SELECT * FROM MSAcpi_ThermalZoneTemperature")` → enumerate.
- RAII-wrap every COM interface (`IWbemLocator`, `IWbemServices`, `IEnumWbemClassObject`) so `Release()` always runs; balance `CoInitializeEx`/`CoUninitialize`.
- **Caveat:** many consumer laptops expose no ACPI thermal zone here → the query returns zero rows. Treat that as "unavailable" and fall back.

## Option B — PDH performance counters
- Use `PdhOpenQuery` / `PdhAddCounter` on a `Thermal Zone Information` counter path, `PdhCollectQueryData`, then `PdhGetFormattedCounterValue`.
- Availability is also hardware-dependent.

## Option C — Fallback: LibreHardwareMonitor
- When A and B yield nothing, read from LibreHardwareMonitorLib (per-core CPU package temps). Load it behind the same `ITemperatureSensor` so `ThermalMonitor` is unaffected.

## Monitor Loop Rules
- Sample on a `std::jthread` at `poll_interval_sec` (default 5s), respecting `std::stop_token`.
- Keep a **rolling average** (e.g., last N samples) and compare the average, not a single spike, against thresholds to avoid hysteresis bouncing.
- On crossing `critical_temp_c`, trigger mitigation against `throttle_targets` (see the win32-process-runner skill for suspend/terminate). Add hysteresis before declaring recovery below `warning_temp_c`.

## Pitfalls
- Forgetting the tenths-of-Kelvin conversion → readings ~2730 too high.
- Calling WMI without `CoInitializeSecurity` → `E_ACCESSDENIED`.
- Treating a single missing/zero read as 0 °C instead of "unavailable".
- Running COM calls on the GUI thread — keep sensing on the telemetry thread and initialize COM there.
