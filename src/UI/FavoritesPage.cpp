#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "UI/Menu.h"

namespace whereabouts::ui
{
    void Menu::RenderFavorites()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        if (ImGuiMCP::BeginChild(
                "##WhereaboutsFavoritesPageScroll",
                {0.0F, 0.0F},
                ImGuiMCP::ImGuiChildFlags_None)) {
        ImGuiMCP::SeparatorText(TranslateText("Favorites"));
        ImGuiMCP::Spacing();
        SyncSelectedTarget();
        const auto entries = savedNpcs_.Favorites();
        const auto toolbarWidth = ImGuiMCP::GetContentRegionAvail().x;
        const auto* style = ImGuiMCP::GetStyle();
        const auto clearWidth = ImGuiMCP::CalcTextSize(TranslateText("Clear Favorites")).x +
            (style ? style->FramePadding.x * 2.0F : 8.0F);
        const bool clearRendered = !entries.empty();
        if (clearRendered && ImGuiMCP::Button(TranslateText("Clear Favorites"))) {
            ImGuiMCP::OpenPopup(TranslateText("Clear all favorites?"));
        }
        RenderSavedListSearch(
            favoriteListSearch_, favoriteListPagination_, "##FavoriteListSearch",
            clearRendered, toolbarWidth, clearWidth);
        RenderSavedEntries(
            entries, TargetSource::Favorite, favoriteListSearch_.data(), favoriteListPagination_);
        CenterNextModal();
        if (ImGuiMCP::BeginPopupModal(
                TranslateText("Clear all favorites?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::TextUnformatted(TranslateText("Remove every favorite from this save?"));
            if (ImGuiMCP::Button(TranslateText("Clear"))) {
                static_cast<void>(favorites_.Clear());
                if (settings_.shareFavoritesAcrossSaves) {
                    settings_.shareFavoritesAcrossSaves = false;
                    SaveSettings();
                    savedEntriesStatus_ = TranslateOwned(
                        "Favorites were cleared from this save. Sharing was turned off and the shared list was preserved.");
                }
                ResetListPaginationForQuery(favoriteListPagination_);
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(TranslateText("Cancel"))) ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
        }
        if (selected_) {
            ImGuiMCP::Separator();
            RenderDetails(settings_.DensityFor(UiDensityArea::SelectedNpc));
        }
        }
        ImGuiMCP::EndChild();
        RenderRootModals();
    }
}
