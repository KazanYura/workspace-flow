// Phase 2: exercises the Win32 Subprocess wrapper against cmd.exe built-ins.
// Requires Windows (uses run_process, which wraps cmd.exe /C).

#include <cstdio>
#include <string>
#include <string_view>

#include "devflow/pipeline/Subprocess.hpp"

using namespace devflow;

namespace {

int g_failures = 0;

void check(bool condition, std::string_view what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %.*s\n", static_cast<int>(what.size()), what.data());
        ++g_failures;
    }
}

}  // namespace

int main() {
    // Zero exit code => success.
    {
        const ProcessResult r = run_process("exit 0");
        check(r.launched, "exit 0 launched");
        check(r.succeeded(), "exit 0 succeeded");
        check(r.exit_code == 0, "exit 0 exit_code");
    }

    // Non-zero exit code is captured, not treated as an error to throw.
    {
        const ProcessResult r = run_process("exit 7");
        check(r.launched, "exit 7 launched");
        check(!r.succeeded(), "exit 7 not success");
        check(r.exit_code == 7, "exit 7 exit_code");
    }

    // stdout is captured.
    {
        const ProcessResult r = run_process("echo devflow-marker");
        check(r.succeeded(), "echo succeeded");
        check(r.output.find("devflow-marker") != std::string::npos, "echo output captured");
    }

    // A long-running child must be killed once the timeout elapses.
    {
        const ProcessResult r = run_process("ping -n 10 127.0.0.1", 1);
        check(r.launched, "timeout case launched");
        check(r.timed_out, "long command timed out");
        check(!r.succeeded(), "timed out not success");
    }

    if (g_failures == 0) {
        std::puts("all tests passed");
        return 0;
    }
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
}
