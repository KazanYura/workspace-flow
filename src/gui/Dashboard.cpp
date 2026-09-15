#include "devflow/gui/Dashboard.hpp"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>

#include <imgui.h>

namespace devflow::gui {
namespace {

ImVec4 task_color(TaskState state) {
    switch (state) {
        case TaskState::Pending: return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
        case TaskState::Running: return ImVec4(0.25f, 0.60f, 1.00f, 1.0f);
        case TaskState::Done:    return ImVec4(0.30f, 0.80f, 0.35f, 1.0f);
        case TaskState::Failed:  return ImVec4(0.90f, 0.30f, 0.30f, 1.0f);
        case TaskState::Skipped: return ImVec4(0.75f, 0.70f, 0.40f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

const char* task_label(TaskState state) {
    switch (state) {
        case TaskState::Pending: return "Pending";
        case TaskState::Running: return "Running";
        case TaskState::Done:    return "Done";
        case TaskState::Failed:  return "Failed";
        case TaskState::Skipped: return "Skipped";
    }
    return "?";
}

ImVec4 thermal_color(ThermalState state) {
    switch (state) {
        case ThermalState::Normal:   return ImVec4(0.30f, 0.80f, 0.35f, 1.0f);
        case ThermalState::Warning:  return ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
        case ThermalState::Critical: return ImVec4(0.90f, 0.30f, 0.30f, 1.0f);
        case ThermalState::Unknown:  return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

const char* thermal_label(ThermalState state) {
    switch (state) {
        case ThermalState::Normal:   return "Normal";
        case ThermalState::Warning:  return "Warning";
        case ThermalState::Critical: return "Critical";
        case ThermalState::Unknown:  return "Unknown";
    }
    return "?";
}

void render_thermal(const DashboardState& state) {
    ImGui::SeparatorText("Thermal");
    ImGui::TextColored(thermal_color(state.thermal), "%.1f \xC2\xB0""C", state.temperature_c);
    ImGui::SameLine();
    ImGui::TextColored(thermal_color(state.thermal), "(%s)", thermal_label(state.thermal));

    const float fraction = std::clamp(state.temperature_c / 100.0f, 0.0f, 1.0f);
    ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.0f), "");
}

void render_tasks(const DashboardState& state) {
    ImGui::SeparatorText("Pipeline");

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("tasks", 3, flags)) {
        ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableHeadersRow();

        for (const TaskRow& task : state.tasks) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(task.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(task.type.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(task_color(task.state), "%s", task_label(task.state));
        }
        ImGui::EndTable();
    }
}

void render_logs(const DashboardState& state) {
    ImGui::SeparatorText("Logs");
    static char filter[128] = {};
    ImGui::InputTextWithHint("##log_filter", "Filter logs", filter, sizeof(filter));
    const std::string_view needle{filter};
    if (ImGui::BeginChild("logs", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border)) {
        std::size_t start = 0;
        while (start <= state.logs.size()) {
            const std::size_t end = state.logs.find('\n', start);
            const std::string_view line = state.logs.substr(
                start, end == std::string::npos ? std::string::npos : end - start);
            if (needle.empty() || line.find(needle) != std::string_view::npos) {
                ImGui::TextUnformatted(line.data(), line.data() + line.size());
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        // Keep the newest lines in view while the user has not scrolled up.
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void render_gate(DashboardState& state) {
    const char* id = state.gate_title.empty() ? "Gate" : state.gate_title.c_str();
    if (state.gate_active) {
        ImGui::OpenPopup(id);
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
        ImGui::TextUnformatted(state.gate_message.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Separator();
        if (ImGui::Button("Continue", ImVec2(120.0f, 0.0f))) {
            state.gate_continue_requested = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void render_toolbar(DashboardState& state) {
    const bool busy = state.running;

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        state.request_open = true;
    }
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R) && !busy &&
        state.has_config) {
        state.request_run = true;
    }

    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Open YAML\xE2\x80\xA6")) {
        state.request_open = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (busy) {
        if (ImGui::Button("Stop")) {
            state.request_stop = true;
        }
    } else {
        ImGui::BeginDisabled(!state.has_config);
        if (ImGui::Button("Run")) {
            state.request_run = true;
        }
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s",
                        state.config_path.empty() ? "(no pipeline loaded)"
                                                  : state.config_path.c_str());

    if (!state.status_message.empty()) {
        ImGui::TextUnformatted(state.status_message.c_str());
    }

    ImGui::SameLine();
    if (ImGui::Button(state.dark_theme ? "Light theme" : "Dark theme")) {
        state.dark_theme = !state.dark_theme;
    }
}

}  // namespace

void render_dashboard(DashboardState& state) {
    static bool previous_dark_theme = true;
    if (state.dark_theme != previous_dark_theme) {
        if (state.dark_theme) {
            ImGui::StyleColorsDark();
        } else {
            ImGui::StyleColorsLight();
        }
        previous_dark_theme = state.dark_theme;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus;

    if (ImGui::Begin("DevFlow Orchestrator", nullptr, flags)) {
        render_toolbar(state);
        render_thermal(state);
        render_tasks(state);
        render_logs(state);
    }
    ImGui::End();

    render_gate(state);
}

}  // namespace devflow::gui
