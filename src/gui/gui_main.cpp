#include <cstdio>

#include "devflow/gui/GuiApp.hpp"
#include "devflow/gui/GuiPipelineController.hpp"

using namespace devflow::gui;

// GUI entry point. The dashboard drives the same execution engine as the
// console app: load a pipeline YAML (via argv or the Open button), press Run,
// and the PipelineRunner + ThermalMonitor execute on background threads while
// the window shows live task status, streamed logs, temperature, and any gate.
int main(int argc, char** argv) {
    GuiApp app("DevFlow Orchestrator", 1100, 720);
    if (!app.valid()) {
        std::fprintf(stderr, "Failed to initialize the DevFlow GUI window.\n");
        return 1;
    }

    GuiPipelineController controller;
    if (argc > 1) {
        controller.queue_load(argv[1]);
    }

    app.run([&controller](DashboardState& state) { controller.update(state); });
    return 0;
}
