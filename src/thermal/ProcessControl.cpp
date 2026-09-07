#include "devflow/thermal/ProcessControl.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace devflow {
namespace {

struct HandleDeleter {
    void operator()(HANDLE h) const noexcept {
        if (h && h != INVALID_HANDLE_VALUE) ::CloseHandle(h);
    }
};
using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleDeleter>;

std::wstring utf8_to_wide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                          static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                          wide.data(), len);
    return wide;
}

// Ordinal, case-insensitive compare avoids CRT locale differences across toolchains.
bool iequals(const wchar_t* a, const wchar_t* b) noexcept {
    return ::CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL;
}

std::vector<DWORD> find_pids(std::string_view image_name) {
    std::vector<DWORD> pids;
    const std::wstring target = utf8_to_wide(image_name);
    if (target.empty()) return pids;

    UniqueHandle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot.get() == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (::Process32FirstW(snapshot.get(), &entry)) {
        do {
            if (iequals(entry.szExeFile, target.c_str())) {
                pids.push_back(entry.th32ProcessID);
            }
        } while (::Process32NextW(snapshot.get(), &entry));
    }
    return pids;
}

// Applies SuspendThread or ResumeThread to every thread owned by `pid`.
void for_each_thread(DWORD pid, DWORD (WINAPI* action)(HANDLE)) {
    UniqueHandle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
    if (snapshot.get() == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (::Thread32First(snapshot.get(), &entry)) {
        do {
            if (entry.th32OwnerProcessID == pid) {
                UniqueHandle thread(::OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID));
                if (thread) action(thread.get());
            }
        } while (::Thread32Next(snapshot.get(), &entry));
    }
}

std::size_t apply_thread_action(std::string_view image_name, DWORD (WINAPI* action)(HANDLE)) {
    const auto pids = find_pids(image_name);
    for (const DWORD pid : pids) {
        for_each_thread(pid, action);
    }
    return pids.size();
}

}  // namespace

std::size_t suspend_processes(std::string_view image_name) {
    return apply_thread_action(image_name, &::SuspendThread);
}

std::size_t resume_processes(std::string_view image_name) {
    return apply_thread_action(image_name, &::ResumeThread);
}

std::size_t terminate_processes(std::string_view image_name) {
    std::size_t affected = 0;
    for (const DWORD pid : find_pids(image_name)) {
        UniqueHandle process(::OpenProcess(PROCESS_TERMINATE, FALSE, pid));
        if (process && ::TerminateProcess(process.get(), 1)) {
            ++affected;
        }
    }
    return affected;
}

}  // namespace devflow
