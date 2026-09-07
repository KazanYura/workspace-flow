#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "devflow/config/Config.hpp"
#include "devflow/config/Task.hpp"
#include "devflow/gui/DashboardState.hpp"
#include "devflow/pipeline/PipelineRunner.hpp"
#include "devflow/thermal/ThermalMonitor.hpp"

namespace devflow::gui {

// Bridges the GUI (a plain DashboardState) to the real execution engine: it
// runs the pipeline and the thermal watchdog on background threads and marshals
// their live state back to the UI thread. It contains no ImGui calls, and the
// renderer contains no engine calls — this class is the only integration seam.
class GuiPipelineController {
public:
    GuiPipelineController();
    ~GuiPipelineController();

    GuiPipelineController(const GuiPipelineController&) = delete;
    GuiPipelineController& operator=(const GuiPipelineController&) = delete;

    // Load a config on the next update() (used for a path passed on argv).
    void queue_load(std::string path);

    // Call once per frame on the UI thread: services the Open/Run/Stop and gate
    // requests raised by the dashboard, then copies live engine state back.
    void update(DashboardState& state);

private:
    void open_dialog(DashboardState& state);
    void load_config(const std::string& path, DashboardState& state);
    void start();
    void stop();
    void sync(DashboardState& state);
    void run_pipeline(const std::vector<Task>& pipeline, std::stop_token stop);

    // Engine-thread callbacks (invoked off the UI thread).
    void on_log(std::string_view line);
    void on_progress(const TaskOutcome& outcome);
    bool wait_gate(const GateTask& gate, std::stop_token stop);
    void mitigate(MitigationAction action, const std::vector<std::string>& targets);

    std::optional<Config> config_;
    std::optional<std::string> pending_path_;
    std::string config_path_;

    std::unique_ptr<ThermalMonitor> monitor_;

    // Guards the task table + gate fields shared with the render thread.
    std::mutex state_mtx_;
    std::vector<TaskRow> tasks_;
    std::unordered_map<std::string, std::size_t> index_by_id_;
    std::string current_task_name_;
    bool gate_active_ = false;
    std::string gate_title_;
    std::string gate_message_;
    bool gate_resolved_ = false;
    bool gate_proceed_ = false;
    std::condition_variable_any gate_cv_;

    std::mutex log_mtx_;
    std::deque<std::string> log_queue_;

    std::atomic<bool> running_{false};
    std::atomic<bool> finished_{false};

    std::jthread worker_;  // declared last: stopped/joined first on destruction
};

}  // namespace devflow::gui
