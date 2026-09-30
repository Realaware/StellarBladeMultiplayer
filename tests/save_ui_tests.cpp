#include "sbcoop/ui/save_panel.hpp"
#include <imgui.h>
#include <iostream>
#include <stdexcept>

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2{900.0F, 600.0F};
    io.DeltaTime = 1.0F / 60.0F;
    unsigned char* pixels = nullptr; int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    try {
        sbcoop::save::RuntimeSnapshot snapshot;
        snapshot.backup = {sbcoop::save::BackupState::verified, std::string(64, 'A'), 1};
        snapshot.creation_enabled = true;
        snapshot.decision = {sbcoop::save::Reason::allowed,
            sbcoop::save::Flag{std::string(32, 'a'), {"account", "coop", "instance", "profile"}}, 1};
        auto mapped = sbcoop::ui::make_save_panel_model(snapshot);
        if (!mapped.creation_enabled || !mapped.multiplayer_allowed || !mapped.flagged) { throw std::runtime_error("ready runtime status was lost"); }
        snapshot.checking = true;
        mapped = sbcoop::ui::make_save_panel_model(snapshot);
        if (mapped.creation_enabled || mapped.multiplayer_allowed || mapped.flagged || !mapped.busy) { throw std::runtime_error("pending verification left UI actions enabled"); }
        snapshot.checking = false; snapshot.failure = sbcoop::save::RuntimeFailure::overflow;
        mapped = sbcoop::ui::make_save_panel_model(snapshot);
        if (mapped.creation_enabled || mapped.multiplayer_allowed || mapped.flagged) { throw std::runtime_error("runtime fault left UI actions enabled"); }
        for (int state = 0; state < 4; ++state) {
            sbcoop::ui::SavePanelModel model;
            model.preview = true;
            model.message = "Waiting for verified native save bindings";
            if (state == 1) { model.backup_verified = true; model.creation_enabled = true; }
            if (state == 2) { model.multiplayer_allowed = true; model.flagged = true; model.loaded_save = "Fixture co-op save"; }
            if (state == 3) { model = sbcoop::ui::make_save_panel_model(snapshot, true); }
            ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2{650.0F, 450.0F});
            ImGui::Begin("Co-op save panel");
            const auto action = sbcoop::ui::draw_save_panel(model);
            if (action != sbcoop::ui::SavePanelAction::none) { throw std::runtime_error("UI issued an unsolicited command"); }
            ImGui::End(); ImGui::Render();
            if (ImGui::GetDrawData()->TotalVtxCount == 0) { throw std::runtime_error("UI produced no draw data"); }
        }
        std::cout << "PASS: Dear ImGui " << ImGui::GetVersion()
                  << " renders blocked, creation-ready, flagged and faulted states without unsolicited actions\n";
        ImGui::DestroyContext(); return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; ImGui::DestroyContext(); return 1; }
}
