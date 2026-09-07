#include "devflow/thermal/WmiTemperatureSensor.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

// _WIN32_DCOM is required for CoInitializeEx / DCOM security on the WMI path.
#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wbemidl.h>

namespace devflow {
namespace {

// Balanced CoInitializeEx / CoUninitialize for a single scope on one thread.
class ComApartment {
public:
    ComApartment() noexcept {
        const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        owns_ = SUCCEEDED(hr);              // only uninit if we did the init
        ready_ = owns_ || hr == RPC_E_CHANGED_MODE;
    }
    ~ComApartment() {
        if (owns_) ::CoUninitialize();
    }
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

    [[nodiscard]] bool ready() const noexcept { return ready_; }

private:
    bool owns_ = false;
    bool ready_ = false;
};

template <typename T>
struct ComReleaser {
    void operator()(T* p) const noexcept {
        if (p) p->Release();
    }
};
template <typename T>
using ComPtr = std::unique_ptr<T, ComReleaser<T>>;

// RAII for a BSTR so early returns never leak the allocation.
struct BStrDeleter {
    void operator()(OLECHAR* s) const noexcept {
        if (s) ::SysFreeString(s);
    }
};
using UniqueBStr = std::unique_ptr<OLECHAR, BStrDeleter>;

UniqueBStr make_bstr(const wchar_t* text) {
    return UniqueBStr{::SysAllocString(text)};
}

std::optional<long> variant_to_long(const VARIANT& v) noexcept {
    switch (v.vt) {
        case VT_I4:   return v.lVal;
        case VT_UI4:  return static_cast<long>(v.ulVal);
        case VT_I2:   return v.iVal;
        case VT_UI2:  return v.uiVal;
        case VT_INT:  return v.intVal;
        case VT_UINT: return static_cast<long>(v.uintVal);
        default:      return std::nullopt;
    }
}

class WmiTemperatureSensor final : public ITemperatureSensor {
public:
    std::optional<double> ReadCelsius() override {
        ComApartment com;
        if (!com.ready()) {
            return std::nullopt;
        }

        // Process-wide; harmless RPC_E_TOO_LATE once already set.
        ::CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                               RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                               nullptr, EOAC_NONE, nullptr);

        IWbemLocator* locator_raw = nullptr;
        if (FAILED(::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_IWbemLocator,
                                      reinterpret_cast<void**>(&locator_raw)))) {
            return std::nullopt;
        }
        ComPtr<IWbemLocator> locator(locator_raw);

        const UniqueBStr ns = make_bstr(L"ROOT\\WMI");
        IWbemServices* services_raw = nullptr;
        if (FAILED(locator->ConnectServer(ns.get(), nullptr, nullptr, nullptr, 0,
                                          nullptr, nullptr, &services_raw))) {
            return std::nullopt;
        }
        ComPtr<IWbemServices> services(services_raw);

        if (FAILED(::CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                       nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                       RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE))) {
            return std::nullopt;
        }

        const UniqueBStr wql = make_bstr(L"WQL");
        const UniqueBStr query =
            make_bstr(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature");
        IEnumWbemClassObject* enum_raw = nullptr;
        if (FAILED(services->ExecQuery(wql.get(), query.get(),
                                       WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                       nullptr, &enum_raw))) {
            return std::nullopt;
        }
        ComPtr<IEnumWbemClassObject> enumerator(enum_raw);

        // Average across every reported thermal zone.
        double sum_celsius = 0.0;
        int count = 0;
        while (true) {
            IWbemClassObject* obj_raw = nullptr;
            ULONG returned = 0;
            if (enumerator->Next(WBEM_INFINITE, 1, &obj_raw, &returned) != WBEM_S_NO_ERROR ||
                returned == 0) {
                break;
            }
            ComPtr<IWbemClassObject> obj(obj_raw);

            VARIANT value;
            ::VariantInit(&value);
            if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &value, nullptr, nullptr))) {
                if (const auto tenths_kelvin = variant_to_long(value)) {
                    sum_celsius += static_cast<double>(*tenths_kelvin) / 10.0 - 273.15;
                    ++count;
                }
            }
            ::VariantClear(&value);
        }

        if (count == 0) {
            return std::nullopt;  // no ACPI thermal zone on this hardware
        }
        return sum_celsius / count;
    }
};

}  // namespace

std::unique_ptr<ITemperatureSensor> make_wmi_sensor() {
    return std::make_unique<WmiTemperatureSensor>();
}

}  // namespace devflow
