// Minimal dependency-free test for the Phase 1 config parser.
// Phases 1-3 keep deps to yaml-cpp + spdlog, so no test framework is pulled in yet.

#include <cstdio>
#include <string_view>
#include <variant>

#include "devflow/config/ConfigParser.hpp"

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
    constexpr std::string_view kValidYaml = R"(
thermal_monitor:
  enabled: true
  poll_interval_sec: 5
  warning_temp_c: 80
  critical_temp_c: 90
  throttle_targets:
    - "chrome.exe"
    - "Code.exe"

pipeline:
  - id: step_cmd
    name: "Run command"
    type: command
    command: "git pull"
    timeout_sec: 30
  - id: step_poll
    name: "Wait for docker"
    type: poll
    command: "docker info"
    retry_interval_sec: 2
    max_retries: 15
  - id: step_gate
    name: "VPN gate"
    type: gate
    message: "Approve 2FA"
    pre_command: "vpnui.exe"
)";

    // Happy path.
    const ParseResult happy = parse_config_string(kValidYaml);
    if (const Config* c = std::get_if<Config>(&happy)) {
        check(c->thermal.enabled, "thermal enabled");
        check(c->thermal.warning_temp_c == 80, "warning_temp_c parsed");
        check(c->thermal.critical_temp_c == 90, "critical_temp_c parsed");
        check(c->thermal.throttle_targets.size() == 2, "two throttle targets");
        check(c->pipeline.size() == 3, "three tasks parsed");

        check(std::holds_alternative<CommandTask>(c->pipeline[0].action), "task 0 is command");
        check(std::holds_alternative<PollTask>(c->pipeline[1].action), "task 1 is poll");
        check(std::holds_alternative<GateTask>(c->pipeline[2].action), "task 2 is gate");

        const auto& cmd = std::get<CommandTask>(c->pipeline[0].action);
        check(cmd.command == "git pull", "command text parsed");
        check(cmd.timeout_sec.has_value() && *cmd.timeout_sec == 30, "timeout_sec parsed");

        const auto& poll = std::get<PollTask>(c->pipeline[1].action);
        check(poll.max_retries == 15, "max_retries parsed");
        check(poll.retry_interval_sec == 2, "retry_interval_sec parsed");

        const auto& gate = std::get<GateTask>(c->pipeline[2].action);
        check(gate.message == "Approve 2FA", "gate message parsed");
        check(gate.pre_command.has_value(), "gate pre_command parsed");
    } else {
        check(false, "valid config should parse");
    }

    // Unknown task type must be rejected, not silently skipped.
    check(std::holds_alternative<ParseError>(
              parse_config_string("pipeline:\n  - id: x\n    type: bogus\n")),
          "unknown task type rejected");

    // Missing required field must be rejected.
    check(std::holds_alternative<ParseError>(
              parse_config_string("pipeline:\n  - id: x\n    type: command\n")),
          "command task without 'command' rejected");

    if (g_failures == 0) {
        std::puts("all tests passed");
        return 0;
    }
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
}
