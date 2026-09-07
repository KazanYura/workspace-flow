// Phase 3: drives the ThermalMonitor with a fake sensor so classification,
// hysteresis, and mitigation dispatch are deterministic (no real hardware or
// processes involved). sample_once() is called directly instead of the thread.

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "devflow/config/ThermalConfig.hpp"
#include "devflow/thermal/ThermalMonitor.hpp"

using namespace devflow;

namespace {

int g_failures = 0;

void check(bool condition, std::string_view what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %.*s\n", static_cast<int>(what.size()), what.data());
        ++g_failures;
    }
}

// Returns scripted readings in order, then nullopt (unavailable) once exhausted.
class ScriptedSensor final : public ITemperatureSensor {
public:
    explicit ScriptedSensor(std::vector<std::optional<double>> values)
        : values_(std::move(values)) {}

    std::optional<double> ReadCelsius() override {
        if (index_ < values_.size()) return values_[index_++];
        return std::nullopt;
    }

private:
    std::vector<std::optional<double>> values_;
    std::size_t index_ = 0;
};

ThermalConfig make_config() {
    ThermalConfig cfg;
    cfg.enabled = true;
    cfg.poll_interval_sec = 1;
    cfg.warning_temp_c = 70;
    cfg.critical_temp_c = 80;
    cfg.throttle_targets = {"target.exe"};
    return cfg;
}

struct Recorded {
    std::vector<MitigationAction> actions;
    std::vector<std::string> targets_seen;
};

}  // namespace

int main() {
    const LogSink silent = [](std::string_view) {};

    // Throttle on critical breach, restore on recovery, each fired exactly once.
    {
        auto sensor = std::make_unique<ScriptedSensor>(std::vector<std::optional<double>>{
            50.0,    // normal
            95.0,    // critical -> throttle
            95.0,    // still critical -> no repeat
            75.0,    // warning band -> stay throttled (hysteresis)
            40.0,    // normal -> restore
        });

        Recorded rec;
        auto mitigate = [&rec](MitigationAction action, const std::vector<std::string>& targets) {
            rec.actions.push_back(action);
            for (const auto& t : targets) rec.targets_seen.push_back(t);
        };

        ThermalMonitor monitor(make_config(), std::move(sensor), silent, mitigate,
                               /*rolling_window=*/1);

        monitor.sample_once();
        check(monitor.level() == ThermalLevel::Normal, "50C is normal");

        monitor.sample_once();
        check(monitor.level() == ThermalLevel::Critical, "95C is critical");
        check(rec.actions.size() == 1, "throttle fired once");
        check(rec.actions.front() == MitigationAction::Throttle, "first action is throttle");

        monitor.sample_once();
        check(rec.actions.size() == 1, "no repeat throttle while already throttled");

        monitor.sample_once();
        check(monitor.level() == ThermalLevel::Warning, "75C is warning band");
        check(rec.actions.size() == 1, "no restore in warning band (hysteresis)");

        monitor.sample_once();
        check(monitor.level() == ThermalLevel::Normal, "40C back to normal");
        check(rec.actions.size() == 2, "restore fired once");
        check(rec.actions.back() == MitigationAction::Restore, "last action is restore");
        check(!rec.targets_seen.empty() && rec.targets_seen.front() == "target.exe",
              "target name forwarded to handler");
    }

    // Rolling average smooths a single spike below the critical threshold.
    {
        auto sensor = std::make_unique<ScriptedSensor>(std::vector<std::optional<double>>{
            60.0, 60.0, 60.0, 100.0,  // avg = 70 -> warning, not critical
        });

        Recorded rec;
        auto mitigate = [&rec](MitigationAction action, const std::vector<std::string>&) {
            rec.actions.push_back(action);
        };

        ThermalMonitor monitor(make_config(), std::move(sensor), silent, mitigate,
                               /*rolling_window=*/4);
        for (int i = 0; i < 4; ++i) monitor.sample_once();

        check(monitor.level() == ThermalLevel::Warning, "spike averaged into warning");
        check(rec.actions.empty(), "no throttle when average stays below critical");
        const auto avg = monitor.last_average();
        check(avg && *avg > 69.0 && *avg < 71.0, "rolling average ~70C");
    }

    // An unavailable sensor never crashes and triggers no mitigation.
    {
        auto sensor = std::make_unique<ScriptedSensor>(
            std::vector<std::optional<double>>{std::nullopt, std::nullopt});

        Recorded rec;
        auto mitigate = [&rec](MitigationAction action, const std::vector<std::string>&) {
            rec.actions.push_back(action);
        };

        ThermalMonitor monitor(make_config(), std::move(sensor), silent, mitigate);
        monitor.sample_once();
        monitor.sample_once();

        check(rec.actions.empty(), "no mitigation when sensor unavailable");
        check(!monitor.last_average().has_value(), "no average recorded without readings");
    }

    if (g_failures == 0) {
        std::puts("all tests passed");
        return 0;
    }
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
}
