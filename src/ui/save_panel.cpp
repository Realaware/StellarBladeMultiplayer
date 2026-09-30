#include "sbcoop/ui/save_panel.hpp"

#include <imgui.h>

namespace sbcoop::ui {

SavePanelModel make_save_panel_model(const save::RuntimeSnapshot& snapshot, bool preview) {
    SavePanelModel model;
    model.preview = preview;
    model.backup_verified = save::valid_backup_check(snapshot.backup);
    const bool ready = snapshot.failure == save::RuntimeFailure::none && !snapshot.checking && !snapshot.busy;
    model.creation_enabled = ready && model.backup_verified && snapshot.creation_enabled;
    model.checking = snapshot.checking;
    model.busy = snapshot.busy || snapshot.checking;
    model.multiplayer_allowed = ready && model.backup_verified && snapshot.decision.allowed() && snapshot.decision.flag.has_value();
    model.flagged = model.multiplayer_allowed;
    model.loaded_save = snapshot.checking ? "Checking..." : snapshot.loaded_slot;
    model.message = snapshot.failure != save::RuntimeFailure::none ? "Save manager stopped. Multiplayer remains locked." :
        snapshot.checking ? "Checking the retained backup and loaded save." :
        snapshot.busy ? "Waiting for native save creation to finish." : std::string(save::describe(snapshot.reason));
    return model;
}

SavePanelAction draw_save_panel(const SavePanelModel& model) {
    auto action = SavePanelAction::none;
    const ImVec4 green{0.36F, 0.86F, 0.65F, 1.0F};
    const ImVec4 amber{0.97F, 0.72F, 0.30F, 1.0F};
    ImGui::TextColored(green, "STELLAR BLADE");
    ImGui::TextUnformatted("Co-op saves");
    if (model.preview) { ImGui::TextColored(amber, "PREVIEW: simulated saves; game integration is unavailable"); }
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(model.multiplayer_allowed ? green : amber,
                       "%s", model.multiplayer_allowed ? "Multiplayer save ready" : "Multiplayer locked");
    ImGui::Text("Backup: %s", model.backup_verified ? "Verified" : "Verification required");
    ImGui::Text("Loaded save: %s", model.loaded_save.c_str());
    ImGui::Text("Multiplayer flag: %s", model.flagged ? "Present and matched" : "Not verified");
    if (!model.message.empty()) { ImGui::TextWrapped("%s", model.message.c_str()); }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("Create a separate save for co-op. Only saves created through this menu can enable multiplayer.");
    ImGui::BeginDisabled(!model.creation_enabled || model.busy);
    if (ImGui::Button(model.checking ? "Checking save status..." : model.busy ? "Creating multiplayer save..." : "Create multiplayer save", ImVec2{260.0F, 38.0F})) {
        action = SavePanelAction::create_multiplayer_save;
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(model.busy);
    if (ImGui::Button("Refresh save status", ImVec2{260.0F, 30.0F})) { action = SavePanelAction::refresh; }
    ImGui::EndDisabled();
    return action;
}

} // namespace sbcoop::ui
