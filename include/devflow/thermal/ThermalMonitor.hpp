#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "devflow/common/Logging.hpp"
#include "devflow/thermal/TemperatureSensor.hpp"
#include "devflow/config/ThermalConfig.hpp"

namespace devflow {

// Classification of the current (averaged) temperature against the thresholds.
enum class ThermalLevel { Normal, Warning, Critical };

[[nodiscard]] std::string_view to_string(ThermalLevel level) noexcept;

// What the monitor asks the host to do with the throttle targets.
enum class MitigationAction { Throttle, Restore };

// The monitor decides *when* to act; the host decides *how* (suspend/terminate).
using MitigationHandler =
    std::function<void(MitigationAction action, const std::vector<std::string>& targets)>;

// Fixed-window rolling mean; smooths spikes so a single hot reading can't trip
// or clear a threshold on its own.
class RollingAverage {
public:
    explicit RollingAverage(std::size_t window) : window_(window == 0 ? 1 : window) {}

    void push(double value) {
        samples_.push_back(value);
        sum_ += value;
        if (samples_.size() > window_) {
            sum_ -= samples_.front();
            samples_.pop_front();
        }
    }

    [[nodiscard]] bool empty() const noexcept { return samples_.empty(); }

    [[nodiscard]] double average() const noexcept {
        return samples_.empty() ? 0.0 : sum_ / static_cast<double>(samples_.size());
    }

private:
    std::size_t window_;
    std::deque<double> samples_;
    double sum_ = 0.0;
};

// Background thermal watchdog (ROADMAP Phase 3). Samples an ITemperatureSensor
// on a std::jthread, keeps a rolling average, and dispatches mitigation on a
// critical breach with hysteresis (only restores once back below the warning
// threshold). All console/UI concerns stay behind the injected LogSink and
// MitigationHandler so the class is unit-testable with fakes.
class ThermalMonitor {
public:
    ThermalMonitor(ThermalConfig config,
                   std::unique_ptr<ITemperatureSensor> sensor,
                   LogSink log,
                   MitigationHandler mitigate,
                   std::size_t rolling_window = 5);
    ~ThermalMonitor();

    ThermalMonitor(const ThermalMonitor&) = delete;
    ThermalMonitor& operator=(const ThermalMonitor&) = delete;

    // Launch the telemetry thread. No-op when disabled, sensorless, or running.
    void start();
    // Request stop and join. Safe to call more than once.
    void stop();

    // One sample -> average -> classify -> mitigate cycle. Runs on the telemetry
    // thread; also public so tests can drive it deterministically.
    void sample_once();

    [[nodiscard]] ThermalLevel level() const noexcept { return level_.load(); }
    [[nodiscard]] std::optional<double> last_average() const noexcept {
        return have_average_.load() ? std::optional<double>{average_.load()} : std::nullopt;
    }

private:
    void run(std::stop_token stop);
    [[nodiscard]] ThermalLevel classify(double average_c) const noexcept;
    void log(std::string_view line) const;

    ThermalConfig config_;
    std::unique_ptr<ITemperatureSensor> sensor_;
    LogSink log_;
    MitigationHandler mitigate_;

    RollingAverage average_window_;
    bool throttled_ = false;
    bool warned_unavailable_ = false;

    std::atomic<ThermalLevel> level_{ThermalLevel::Normal};
    std::atomic<double> average_{0.0};
    std::atomic<bool> have_average_{false};

    std::mutex sleep_mutex_;
    std::condition_variable_any sleep_cv_;

    // Declared last so its destructor (stop + join) runs before other members die.
    std::jthread worker_;
};

}  // namespace devflow
