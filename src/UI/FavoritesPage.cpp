#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "UI/Menu.h"

namespace whereabouts::ui
{
    void Menu::RenderFavorites()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        ImGuiMCP::SeparatorText(TranslateText("Favorites"));
        ImGuiMCP::Spacing();
        SyncSelectedTarget();
        const auto entries = savedNpcs_.Favorites();
        if (!entries.empty() && ImGuiMCP::Button(TranslateText("Clear Favorites"))) {
            ImGuiMCP::OpenPopup(TranslateText("Clear all favorites?"));
        }
        RenderSavedEntries(entries, TargetSource::Favorite);
        CenterNextModal();
        if (ImGuiMCP::BeginPopupModal(
                TranslateText("Clear all favorites?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::TextUnformatted(TranslateText("Remove every favorite from this save?"));
            if (ImGuiMCP::Button(TranslateText("Clear"))) {
                savedNpcs_.ClearFavorites();
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
