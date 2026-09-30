#pragma once

#include "sbcoop/save/save_system.hpp"
#include "sbcoop/save/runtime_controller.hpp"

#include <string>

namespace sbcoop::ui {

struct SavePanelModel {
    bool preview{};
    bool backup_verified{};
    bool creation_enabled{};
    bool busy{};
    bool checking{};
    bool multiplayer_allowed{};
    bool flagged{};
    std::string loaded_save{"Unknown"};
    std::string message;
};

enum class SavePanelAction { none, create_multiplayer_save, refresh };
SavePanelModel make_save_panel_model(const save::RuntimeSnapshot& snapshot, bool preview = false);
// UI callers provide copied values. Buttons return commands for a bounded queue.
SavePanelAction draw_save_panel(const SavePanelModel& model);

} // namespace sbcoop::ui
