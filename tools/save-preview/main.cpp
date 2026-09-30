#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "sbcoop/save/file_flag_store.hpp"
#include "sbcoop/ui/save_panel.hpp"

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace sbcoop;
namespace {
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> device_context;
ComPtr<IDXGISwapChain> swap_chain;
ComPtr<ID3D11RenderTargetView> render_target;
bool make_render_target() {
    ComPtr<ID3D11Texture2D> texture;
    return SUCCEEDED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&texture))) &&
           SUCCEEDED(device->CreateRenderTargetView(texture.Get(), nullptr, &render_target));
}
bool make_device(HWND window) {
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL level{};
    const auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &description, &swap_chain, &device, &level, &device_context);
    return SUCCEEDED(result) && make_render_target();
}
std::string random_id() {
    std::array<unsigned char, 16> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("Unable to create a save request ID");
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string id;
    for (const auto byte : bytes) { id += digits[byte >> 4]; id += digits[byte & 15]; }
    return id;
}

// This provider is deliberately confined to the preview executable. No native
// game slot names, creation APIs, or save bytes are assumed by these fixtures.
class FixturePreview {
public:
    FixturePreview() : store_(std::filesystem::current_path() / "out" / "local" / "save-ui-preview"), system_(store_) {
        observation_ = {1, true, true, true, true, "fixture-account", "fixture-profile", save::LoadedState::none,
                        std::nullopt, {{"fixture-account", "solo", "solo-instance", "fixture-profile"}}};
        for (const auto& flag : store_.read_all()) {
            if (flag.identity.account == observation_.account && flag.identity.profile == observation_.profile) {
                observation_.saves.push_back(flag.identity);
            }
        }
    }
    void render() {
        if (ImGui::Checkbox("Enable simulated save provider", &fixtures_enabled_)) { refresh(); }
        ImGui::TextWrapped("This preview writes only fixture metadata under out/local/save-ui-preview. It cannot create or load a Stellar Blade save.");
        ImGui::Spacing();
        ui::SavePanelModel model;
        model.preview = true;
        model.backup_verified = fixtures_enabled_;
        model.creation_enabled = fixtures_enabled_ && observation_.state == save::LoadedState::none && observation_.saves.size() < 64;
        model.multiplayer_allowed = decision_.allowed();
        model.flagged = decision_.flag.has_value();
        model.loaded_save = fixtures_enabled_ ? (observation_.loaded ? observation_.loaded->slot : "None (fixture menu)") : "Unknown";
        model.message = fixtures_enabled_ ? message_ : "Game integration awaits verified loader and native save bindings.";
        switch (ui::draw_save_panel(model)) {
        case ui::SavePanelAction::create_multiplayer_save: create_fixture(); break;
        case ui::SavePanelAction::refresh: refresh(); break;
        case ui::SavePanelAction::none: break;
        }
        if (fixtures_enabled_) {
            ImGui::Spacing(); ImGui::Separator();
            ImGui::TextUnformatted("Fixture controls");
            if (ImGui::Button("Return to fixture menu")) {
                ++observation_.generation; observation_.state = save::LoadedState::none; observation_.loaded.reset(); refresh();
            }
            for (const auto& identity : observation_.saves) {
                ImGui::PushID(identity.slot.c_str());
                if (ImGui::Button(("Load " + identity.slot).c_str())) {
                    ++observation_.generation; observation_.state = save::LoadedState::loaded; observation_.loaded = identity; refresh();
                }
                ImGui::PopID();
            }
        }
    }
private:
    save::BackupCheck backup() { return {save::BackupState::verified, std::string(64, 'A'), ++backup_check_}; }
    void refresh() {
        auto observed = observation_;
        observed.read_binding_verified = fixtures_enabled_;
        decision_ = system_.check_loaded(observed, fixtures_enabled_ ? backup() : save::BackupCheck{});
        message_ = save::describe(decision_.reason);
    }
    void create_fixture() {
        const auto id = random_id();
        const auto reason = system_.begin_creation(observation_, backup(), id);
        if (reason != save::Reason::allowed) { message_ = save::describe(reason); return; }
        const save::Identity created{observation_.account, "fixture-coop-" + id.substr(0, 6), id, observation_.profile};
        observation_.saves.push_back(created);
        ++observation_.generation;
        observation_.state = save::LoadedState::loaded;
        observation_.loaded = created;
        const auto finish = system_.finish_creation({id, true, true, created}, observation_);
        if (finish != save::Reason::allowed) { message_ = save::describe(finish); return; }
        refresh();
    }
    save::FileFlagStore store_;
    save::SaveSystem system_;
    save::Observation observation_;
    save::Decision decision_;
    std::uint64_t backup_check_{};
    bool fixtures_enabled_{};
    std::string message_;
};
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI window_proc(HWND window, UINT message, WPARAM first, LPARAM second) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, first, second)) { return 1; }
    if (message == WM_SIZE && device && first != SIZE_MINIMIZED) {
        render_target.Reset();
        if (SUCCEEDED(swap_chain->ResizeBuffers(0, LOWORD(second), HIWORD(second), DXGI_FORMAT_UNKNOWN, 0))) { make_render_target(); }
        return 0;
    }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, first, second);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class); window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance; window_class.lpszClassName = L"SbCoopSavePreview";
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&window_class)) { return 1; }
    const auto window = CreateWindowExW(0, window_class.lpszClassName, L"Stellar Blade Co-op — Save UI Preview",
        WS_OVERLAPPEDWINDOW, 100, 100, 850, 680, nullptr, nullptr, instance, nullptr);
    if (!window || !make_device(window)) { if (window) { DestroyWindow(window); } UnregisterClassW(window_class.lpszClassName, instance); return 1; }
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2{24.0F, 20.0F}; style.ItemSpacing = ImVec2{12.0F, 10.0F};
    style.FramePadding = ImVec2{12.0F, 7.0F}; style.FrameRounding = 5.0F;
    if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX11_Init(device.Get(), device_context.Get())) { return 1; }
    ShowWindow(window, SW_SHOW); UpdateWindow(window);
    int result = 0;
    try {
        FixturePreview preview;
        bool quit = false;
        while (!quit) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
                if (message.message == WM_QUIT) { quit = true; }
            }
            if (quit) { break; }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2{0, 0}); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Save panel", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            preview.render(); ImGui::End(); ImGui::Render();
            if (render_target) {
                auto* target = render_target.Get();
                device_context->OMSetRenderTargets(1, &target, nullptr);
                constexpr float clear[]{0.08F, 0.09F, 0.12F, 1.0F};
                device_context->ClearRenderTargetView(target, clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); swap_chain->Present(1, 0);
            }
        }
    } catch (const std::exception& error) { MessageBoxA(window, error.what(), "Save preview error", MB_OK); result = 1; }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    render_target.Reset(); swap_chain.Reset(); device_context.Reset(); device.Reset();
    DestroyWindow(window); UnregisterClassW(window_class.lpszClassName, instance);
    return result;
}
