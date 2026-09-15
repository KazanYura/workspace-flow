#include "devflow/pipeline/Subprocess.hpp"

#include <array>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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

std::string win32_error(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = ::FormatMessageW(flags, nullptr, code, 0,
                                          reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) {
        return "Win32 error " + std::to_string(code);
    }
    std::wstring message(buffer, length);
    ::LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    std::string result;
    result.reserve(message.size());
    for (const wchar_t character : message) {
        result.push_back(character >= 0 && character <= 127 ? static_cast<char>(character) : '?');
    }
    return result + " (" + std::to_string(code) + ")";
}

}  // namespace

ProcessResult run_process(std::string_view command_line, std::optional<int> timeout_sec,
                          bool shell) {
    ProcessResult result;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_raw = nullptr;
    HANDLE write_raw = nullptr;
    if (!::CreatePipe(&read_raw, &write_raw, &sa, 0)) {
        result.error = "CreatePipe failed: " + win32_error(::GetLastError());
        return result;
    }
    UniqueHandle read_end(read_raw);
    UniqueHandle write_end(write_raw);

    // Keep the parent's read end out of the child; only the write end is inherited.
    ::SetHandleInformation(read_end.get(), HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_end.get();
    si.hStdError = write_end.get();

    std::wstring cmd = shell ? L"cmd.exe /C " + utf8_to_wide(command_line)
                             : utf8_to_wide(command_line);

    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessW(nullptr, cmd.data(), nullptr, nullptr,
                                     TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        result.error = "CreateProcessW failed: " + win32_error(::GetLastError());
        return result;
    }
    UniqueHandle process(pi.hProcess);
    UniqueHandle thread(pi.hThread);
    result.launched = true;

    // Release the parent's write handle so the reader observes EOF on child exit.
    write_end.reset();

    // Drain the pipe on a worker so a chatty child can't deadlock on a full buffer.
    std::string captured;
    std::jthread reader([raw = read_end.get(), &captured] {
        std::array<char, 4096> buf{};
        DWORD read = 0;
        while (::ReadFile(raw, buf.data(), static_cast<DWORD>(buf.size()), &read, nullptr) &&
               read > 0) {
            captured.append(buf.data(), read);
        }
    });

    const bool bounded = timeout_sec && *timeout_sec > 0;
    const DWORD timeout_ms = bounded ? static_cast<DWORD>(*timeout_sec) * 1000u : INFINITE;
    if (::WaitForSingleObject(process.get(), timeout_ms) == WAIT_TIMEOUT) {
        ::TerminateProcess(process.get(), 1);
        result.timed_out = true;
        ::WaitForSingleObject(process.get(), INFINITE);
    }

    reader.join();  // join before read_end is closed by RAII

    DWORD code = 0;
    if (::GetExitCodeProcess(process.get(), &code)) {
        result.exit_code = code;
    }
    result.output = std::move(captured);
    return result;
}

bool launch_detached(std::string_view command_line) {
    std::wstring cmd = utf8_to_wide(command_line);
    if (cmd.empty()) return false;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!::CreateProcessW(nullptr, cmd.data(), nullptr, nullptr,
                          FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    UniqueHandle process(pi.hProcess);  // close both handles; we never wait
    UniqueHandle thread(pi.hThread);
    return true;
}

}  // namespace devflow
