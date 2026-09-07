#include "devflow/gui/GuiPipelineController.hpp"

#include <format>
#include <utility>
#include <variant>

#include "devflow/config/ConfigParser.hpp"
#include "devflow/thermal/ProcessControl.hpp"
#include "devflow/thermal/WmiTemperatureSensor.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

namespace devflow::gui {
namespace {

constexpr std::size_t kMaxLogChars = 200000;  // trim the oldest output past this

TaskState map_status(TaskStatus status) noexcept {
    switch (status) {
        case TaskStatus::Pending:   return TaskState::Pending;
        case TaskStatus::Running:   return TaskState::Running;
        case TaskStatus::Succeeded: return TaskState::Done;
        case TaskStatus::Failed:    return TaskState::Failed;
        case TaskStatus::Skipped:   return TaskState::Skipped;
    }
    return TaskState::Pending;
}

ThermalState map_level(ThermalLevel level) noexcept {
    switch (level) {
        case ThermalLevel::Normal:   return ThermalState::Normal;
        case ThermalLevel::Warning:  return ThermalState::Warning;
        case ThermalLevel::Critical: return ThermalState::Critical;
    }
    return ThermalState::Unknown;
}

// Native "open file" dialog. Returns a UTF-8 path, or nullopt if cancelled.
std::optional<std::string> pick_yaml_file() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"YAML files\0*.yaml;*.yml\0All files\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select pipeline YAML";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn) == FALSE) {
        return std::nullopt;
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) {
        return std::nullopt;
    }
    std::string utf8(static_cast<std::size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8.data(), len, nullptr, nullptr);
    return utf8;
#else
    return std::nullopt;
#endif
}

}  // namespace

GuiPipelineController::GuiPipelineController() = default;

GuiPipelineController::~GuiPipelineController() {
    stop();
}

void GuiPipelineController::queue_load(std::string path) {
    pending_path_ = std::move(path);
}

void GuiPipelineController::update(DashboardState& state) {
    if (pending_path_) {
        const std::string path = *pending_path_;
        pending_path_.reset();
        load_config(path, state);
    }

    if (state.request_open) {
        state.request_open = false;
        open_dialog(state);
    }
    if (state.request_run) {
        state.request_run = false;
        start();
    }
    if (state.request_stop) {
        state.request_stop = false;
        stop();
    }

    if (state.gate_continue_requested) {
        state.gate_continue_requested = false;
        {
            std::lock_guard lock(state_mtx_);
            if (gate_active_) {
                gate_resolved_ = true;
                gate_proceed_ = true;
            }
        }
        gate_cv_.notify_all();
    }

    sync(state);
}

void GuiPipelineController::open_dialog(DashboardState& state) {
    if (running_.load()) {
        return;
    }
    if (const auto path = pick_yaml_file()) {
        load_config(*path, state);
    }
}

void GuiPipelineController::load_config(const std::string& path, DashboardState& state) {
    ParseResult result = parse_config_file(path);
    if (const auto* error = std::get_if<ParseError>(&result)) {
        state.status_message = std::format("Parse error: {}", error->message);
        return;
    }

    config_ = std::move(std::get<Config>(result));
    config_path_ = path;

    std::lock_guard lock(state_mtx_);
    tasks_.clear();
    index_by_id_.clear();
    for (const Task& task : config_->pipeline) {
        index_by_id_.emplace(task.id, tasks_.size());
        tasks_.push_back({task.name, std::string(task_type_name(task.action)), TaskState::Pending});
    }
    finished_.store(false);
    state.status_message = std::format("Loaded {} task(s) from {}", tasks_.size(), path);
}

void GuiPipelineController::start() {
    if (running_.load() || !config_) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();  // reap a previously finished run before restarting
    }

    {
        std::lock_guard lock(state_mtx_);
        for (TaskRow& row : tasks_) {
            row.state = TaskState::Pending;
        }
        gate_active_ = false;
        gate_resolved_ = false;
        gate_proceed_ = false;
        current_task_name_.clear();
    }
    finished_.store(false);
    running_.store(true);

    monitor_ = std::make_unique<ThermalMonitor>(
        config_->thermal, make_wmi_sensor(),
        [this](std::string_view line) { on_log(line); },
        [this](MitigationAction action, const std::vector<std::string>& targets) {
            mitigate(action, targets);
        });
    monitor_->start();

    std::vector<Task> pipeline = config_->pipeline;
    worker_ = std::jthread(
        [this, pipeline = std::move(pipeline)](std::stop_token stop) { run_pipeline(pipeline, stop); });
}

void GuiPipelineController::stop() {
    if (worker_.joinable()) {
        worker_.request_stop();
        gate_cv_.notify_all();  // release a gate blocked waiting for the user
        worker_.join();
    }
    if (monitor_) {
        monitor_->stop();
        monitor_.reset();
    }
    running_.store(false);
}

void GuiPipelineController::run_pipeline(const std::vector<Task>& pipeline, std::stop_token stop) {
    PipelineRunner runner(
        [this](std::string_view line) { on_log(line); },
        [this, stop](const GateTask& gate) { return wait_gate(gate, stop); },
        [this](const TaskOutcome& outcome) { on_progress(outcome); });

    const PipelineResult result = runner.run(pipeline, stop);
    on_log(result.ok ? "[pipeline] SUCCEEDED" : "[pipeline] FAILED");
    finished_.store(true);
    running_.store(false);
}

void GuiPipelineController::on_log(std::string_view line) {
    std::lock_guard lock(log_mtx_);
    log_queue_.emplace_back(line);
}

void GuiPipelineController::on_progress(const TaskOutcome& outcome) {
    std::lock_guard lock(state_mtx_);
    const auto it = index_by_id_.find(outcome.id);
    if (it == index_by_id_.end()) {
        return;
    }
    TaskRow& row = tasks_[it->second];
    row.state = map_status(outcome.status);
    if (outcome.status == TaskStatus::Running) {
        current_task_name_ = row.name;
    }
}

bool GuiPipelineController::wait_gate(const GateTask& gate, std::stop_token stop) {
    std::unique_lock lock(state_mtx_);
    gate_active_ = true;
    gate_title_ = current_task_name_.empty() ? "Gate" : current_task_name_;
    gate_message_ = gate.message;
    gate_resolved_ = false;
    gate_proceed_ = false;

    gate_cv_.wait(lock, stop, [this] { return gate_resolved_; });

    gate_active_ = false;
    return gate_resolved_ && gate_proceed_ && !stop.stop_requested();
}

void GuiPipelineController::mitigate(MitigationAction action,
                                     const std::vector<std::string>& targets) {
    const bool throttle = action == MitigationAction::Throttle;
    for (const std::string& name : targets) {
        const std::size_t count =
            throttle ? suspend_processes(name) : resume_processes(name);
        on_log(std::format("[thermal] {} {} process(es) named '{}'",
                           throttle ? "suspended" : "resumed", count, name));
    }
}

void GuiPipelineController::sync(DashboardState& state) {
    {
        std::lock_guard lock(log_mtx_);
        while (!log_queue_.empty()) {
            state.logs += log_queue_.front();
            state.logs.push_back('\n');
            log_queue_.pop_front();
        }
    }
    if (state.logs.size() > kMaxLogChars) {
        state.logs.erase(0, state.logs.size() - kMaxLogChars);
    }

    {
        std::lock_guard lock(state_mtx_);
        state.tasks = tasks_;
        state.gate_active = gate_active_;
        state.gate_title = gate_title_;
        state.gate_message = gate_message_;
    }

    state.running = running_.load();
    state.pipeline_finished = finished_.load();
    state.has_config = config_.has_value();
    state.config_path = config_path_;

    if (monitor_) {
        if (const auto average = monitor_->last_average()) {
            state.temperature_c = static_cast<float>(*average);
            state.thermal = map_level(monitor_->level());
        } else {
            state.thermal = ThermalState::Unknown;
        }
    } else {
        state.thermal = ThermalState::Unknown;
    }
}

}  // namespace devflow::gui
