#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "UI/Menu.h"

namespace whereabouts::ui
{
    void Menu::RenderRecent()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        ImGuiMCP::SeparatorText(TranslateText("Recent NPCs"));
        ImGuiMCP::Spacing();
        SyncSelectedTarget();
        const auto entries = savedNpcs_.Recent();
        if (!entries.empty() && ImGuiMCP::Button(TranslateText("Clear Recent"))) {
            ImGuiMCP::OpenPopup(TranslateText("Clear recent history?"));
        }
        RenderSavedEntries(entries, TargetSource::Recent);
        CenterNextModal();
        if (ImGuiMCP::BeginPopupModal(
            TranslateText("Clear recent history?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::TextUnformatted(TranslateText("Remove all recent NPCs from this save?"));
            if (ImGuiMCP::Button(TranslateText("Clear"))) {
                savedNpcs_.ClearRecent();
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(TranslateText("Cancel"))) ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
        }
        if (selected_) {
            ImGuiMCP::Separator();
            RenderDetails();
        }
        RenderRootModals();
    }
}
