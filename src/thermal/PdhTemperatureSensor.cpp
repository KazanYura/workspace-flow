#include "devflow/thermal/PdhTemperatureSensor.hpp"

#include <optional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>

namespace devflow {
namespace {

class PdhTemperatureSensor final : public ITemperatureSensor {
public:
    PdhTemperatureSensor() {
        if (PdhOpenQueryW(nullptr, 0, &query_) != ERROR_SUCCESS) return;
        constexpr const wchar_t* paths[] = {
            L"\\Thermal Zone Information(_Total)\\Temperature",
            L"\\Thermal Zone Information(*)\\Temperature",
        };
        for (const auto* path : paths) {
            if (PdhAddEnglishCounterW(query_, path, 0, &counter_) == ERROR_SUCCESS) break;
        }
        if (!counter_) {
            PdhCloseQuery(query_);
            query_ = nullptr;
            return;
        }
        PdhCollectQueryData(query_);
    }

    ~PdhTemperatureSensor() override {
        if (query_) PdhCloseQuery(query_);
    }

    std::optional<double> ReadCelsius() override {
        if (!query_ || !counter_ || PdhCollectQueryData(query_) != ERROR_SUCCESS) {
            return std::nullopt;
        }
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(counter_, PDH_FMT_DOUBLE, nullptr, &value) !=
            ERROR_SUCCESS) {
            return std::nullopt;
        }
        if (value.doubleValue <= 0.0) return std::nullopt;
        return value.doubleValue > 200.0 ? value.doubleValue / 10.0 - 273.15
                                         : value.doubleValue;
    }

private:
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER counter_ = nullptr;
};

}  // namespace

std::unique_ptr<ITemperatureSensor> make_pdh_sensor() {
    return std::make_unique<PdhTemperatureSensor>();
}

}  // namespace devflow