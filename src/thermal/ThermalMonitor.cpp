#include "devflow/thermal/ThermalMonitor.hpp"

#include <chrono>
#include <format>
#include <utility>

namespace devflow {

std::string_view to_string(ThermalLevel level) noexcept {
    switch (level) {
        case ThermalLevel::Normal:   return "normal";
        case ThermalLevel::Warning:  return "warning";
        case ThermalLevel::Critical: return "critical";
    }
    return "unknown";
}

ThermalMonitor::ThermalMonitor(ThermalConfig config,
                               std::unique_ptr<ITemperatureSensor> sensor,
                               LogSink log,
                               MitigationHandler mitigate,
                               std::size_t rolling_window)
    : config_(std::move(config)),
      sensor_(std::move(sensor)),
      log_(std::move(log)),
      mitigate_(std::move(mitigate)),
      average_window_(rolling_window) {}

ThermalMonitor::~ThermalMonitor() {
    stop();
}

void ThermalMonitor::log(std::string_view line) const {
    if (log_) log_(line);
}

ThermalLevel ThermalMonitor::classify(double average_c) const noexcept {
    if (average_c >= config_.critical_temp_c) return ThermalLevel::Critical;
    if (average_c >= config_.warning_temp_c) return ThermalLevel::Warning;
    return ThermalLevel::Normal;
}

void ThermalMonitor::start() {
    if (!config_.enabled) {
        log("[thermal] monitoring disabled in config");
        return;
    }
    if (!sensor_) {
        log("[thermal] no temperature sensor available");
        return;
    }
    if (worker_.joinable()) {
        return;
    }
    log(std::format("[thermal] monitoring started (poll {}s, warn {}C, crit {}C, {} target(s))",
                    config_.poll_interval_sec, config_.warning_temp_c,
                    config_.critical_temp_c, config_.throttle_targets.size()));
    worker_ = std::jthread([this](std::stop_token stop) { run(std::move(stop)); });
}

void ThermalMonitor::stop() {
    if (!worker_.joinable()) {
        return;
    }
    worker_.request_stop();
    sleep_cv_.notify_all();
    worker_.join();
    log("[thermal] monitoring stopped");
}

void ThermalMonitor::run(std::stop_token stop) {
    const int interval = config_.poll_interval_sec > 0 ? config_.poll_interval_sec : 5;
    while (!stop.stop_requested()) {
        sample_once();
        std::unique_lock lock(sleep_mutex_);
        sleep_cv_.wait_for(lock, stop, std::chrono::seconds(interval),
                           [&stop] { return stop.stop_requested(); });
    }
}

void ThermalMonitor::sample_once() {
    const std::optional<double> reading = sensor_ ? sensor_->ReadCelsius() : std::nullopt;
    if (!reading) {
        if (!warned_unavailable_) {
            log("[thermal] sensor unavailable (no ACPI thermal zone) - monitoring paused");
            warned_unavailable_ = true;
        }
        return;
    }
    if (warned_unavailable_) {
        log("[thermal] sensor reading restored");
        warned_unavailable_ = false;
    }

    average_window_.push(*reading);
    const double average = average_window_.average();
    const ThermalLevel previous = level_.load();
    const ThermalLevel current = classify(average);

    log(std::format("[thermal] {:.1f}C (avg {:.1f}C) [{}]",
                    *reading, average, to_string(current)));

    if (current == ThermalLevel::Critical && !throttled_) {
        log(std::format("[thermal] CRITICAL: avg {:.1f}C >= {}C",
                        average, config_.critical_temp_c));
        if (mitigate_ && !config_.throttle_targets.empty()) {
            mitigate_(MitigationAction::Throttle, config_.throttle_targets);
            throttled_ = true;
        }
    } else if (current == ThermalLevel::Warning && previous == ThermalLevel::Normal &&
               !throttled_) {
        log(std::format("[thermal] WARNING: avg {:.1f}C >= {}C",
                        average, config_.warning_temp_c));
    } else if (current == ThermalLevel::Normal && throttled_) {
        log(std::format("[thermal] recovered: avg {:.1f}C < {}C - restoring targets",
                        average, config_.warning_temp_c));
        if (mitigate_) {
            mitigate_(MitigationAction::Restore, config_.throttle_targets);
        }
        throttled_ = false;
    }

    level_.store(current);
    average_.store(average);
    have_average_.store(true);
}

}  // namespace devflow
