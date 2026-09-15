#include "devflow/system/Autostart.hpp"

#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace devflow {

bool set_autostart(bool enabled, std::string& error) {
#ifdef _WIN32
    HKEY key = nullptr;
    constexpr wchar_t kRunKey[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kValueName[] = L"DevFlowOrchestrator";
    LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key);
    if (status != ERROR_SUCCESS) {
        error = "RegOpenKeyExW failed: " + std::to_string(status);
        return false;
    }

    bool success = false;
    if (!enabled) {
        status = RegDeleteValueW(key, kValueName);
        success = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    } else {
        wchar_t executable[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            error = "GetModuleFileNameW failed";
        } else {
            std::wstring command = L"\"";
            command.append(executable, length);
            command += L"\"";
            status = RegSetValueExW(key, kValueName, 0, REG_SZ,
                                    reinterpret_cast<const BYTE*>(command.c_str()),
                                    static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
            success = status == ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    if (!success && error.empty()) {
        error = (enabled ? "RegSetValueExW" : "RegDeleteValueW") +
                std::string{" failed: "} + std::to_string(status);
    }
    return success;
#else
    error = "auto-start is only supported on Windows";
    return false;
#endif
}

}