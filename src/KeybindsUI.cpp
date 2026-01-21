#include "header/KeybindsUI.h"
#include "external/imgui/imgui.h"

static std::string g_configPath;
static ImGuiKey* g_rebindingTarget = nullptr;

static ImGuiKey CaptureAnyPressedKey() {
    for (ImGuiKey key = (ImGuiKey)ImGuiKey_NamedKey_BEGIN;
         key < (ImGuiKey)ImGuiKey_NamedKey_END;
         key = (ImGuiKey)(key + 1))
    {
        if (ImGui::IsKeyPressed(key, false))
            return key;
    }
    return ImGuiKey_None;
}

static const char* KeyName(ImGuiKey k) {
    const char* n = ImGui::GetKeyName(k);
    return (n && n[0]) ? n : "None";
}

namespace KeybindsUI {

void Init(const std::string& configPath)
{
    g_configPath = configPath;
}

bool DrawPopup(Keybinds& binds, bool& openPopup)
{
    bool saved = false;

    if (openPopup) {
        ImGui::OpenPopup("Keybinds");
        openPopup = false;
    }

    if (ImGui::BeginPopupModal("Keybinds", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

        auto Row = [&](const char* label, ImGuiKey& keyRef) {
            ImGui::Text("%s", label);
            ImGui::SameLine(180);

            std::string btnText = std::string(KeyName(keyRef));
            if (g_rebindingTarget == &keyRef) btnText = "Press a key...";

            if (ImGui::Button((btnText + "##" + label).c_str(), ImVec2(180, 0))) {
                g_rebindingTarget = &keyRef;
            }
        };

        ImGui::Text("NES Controls");
        ImGui::Separator();
        Row("Up", binds.up);
        Row("Down", binds.down);
        Row("Left", binds.left);
        Row("Right", binds.right);
        Row("A", binds.A);
        Row("B", binds.B);
        Row("Start", binds.start);
        Row("Select", binds.select);

        ImGui::Spacing();
        ImGui::Text("Emulator Shortcuts");
        ImGui::Separator();
        Row("Run/Pause", binds.runGame);
        Row("Reset", binds.resetGame);

        if (g_rebindingTarget) {
            ImGuiKey pressed = CaptureAnyPressedKey();
            if (pressed != ImGuiKey_None) {
                *g_rebindingTarget = pressed;
                g_rebindingTarget = nullptr;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                g_rebindingTarget = nullptr;
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            SaveKeybinds(binds, g_configPath);
            saved = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Defaults", ImVec2(120, 0))) {
            binds = Keybinds::Defaults();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            LoadKeybinds(binds, g_configPath);
            g_rebindingTarget = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return saved;
}

} // namespace KeybindsUI
