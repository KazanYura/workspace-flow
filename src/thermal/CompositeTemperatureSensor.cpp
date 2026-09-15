#include "devflow/thermal/CompositeTemperatureSensor.hpp"

#include "devflow/thermal/PdhTemperatureSensor.hpp"
#include "devflow/thermal/WmiTemperatureSensor.hpp"

namespace devflow {
namespace {

class CompositeTemperatureSensor final : public ITemperatureSensor {
public:
    CompositeTemperatureSensor()
        : primary_(make_wmi_sensor()), fallback_(make_pdh_sensor()) {}

    std::optional<double> ReadCelsius() override {
        if (primary_) {
            if (const auto reading = primary_->ReadCelsius()) return reading;
        }
        return fallback_ ? fallback_->ReadCelsius() : std::nullopt;
    }

private:
    std::unique_ptr<ITemperatureSensor> primary_;
    std::unique_ptr<ITemperatureSensor> fallback_;
};

}  // namespace

std::unique_ptr<ITemperatureSensor> make_temperature_sensor() {
    return std::make_unique<CompositeTemperatureSensor>();
}

}  // namespace devflow