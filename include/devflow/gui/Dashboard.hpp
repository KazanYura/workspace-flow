#pragma once

#include "devflow/gui/DashboardState.hpp"

namespace devflow::gui {

// Emits the full dashboard for the current ImGui frame. UI-only: it reads the
// snapshot and, for the gate, writes back gate_continue_requested when the user
// clicks Continue. It performs no system work and starts no threads.
void render_dashboard(DashboardState& state);

}  // namespace devflow::gui
