#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "UI/Menu.h"

namespace whereabouts::ui
{
    void Menu::RenderRecent()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        if (ImGuiMCP::BeginChild(
                "##WhereaboutsRecentPageScroll",
                {0.0F, 0.0F},
                ImGuiMCP::ImGuiChildFlags_None)) {
        ImGuiMCP::SeparatorText(TranslateText("Recent NPCs"));
        ImGuiMCP::Spacing();
        SyncSelectedTarget();
        const auto entries = savedNpcs_.Recent();
        const auto toolbarWidth = ImGuiMCP::GetContentRegionAvail().x;
        const auto* style = ImGuiMCP::GetStyle();
        const auto clearWidth = ImGuiMCP::CalcTextSize(TranslateText("Clear Recent")).x +
            (style ? style->FramePadding.x * 2.0F : 8.0F);
        const bool clearRendered = !entries.empty();
        if (clearRendered && ImGuiMCP::Button(TranslateText("Clear Recent"))) {
            ImGuiMCP::OpenPopup(TranslateText("Clear recent history?"));
        }
        RenderSavedListSearch(
            recentListSearch_, recentListPagination_, "##RecentListSearch",
            clearRendered, toolbarWidth, clearWidth);
        RenderSavedEntries(
            entries, TargetSource::Recent, recentListSearch_.data(), recentListPagination_);
        CenterNextModal();
        if (ImGuiMCP::BeginPopupModal(
            TranslateText("Clear recent history?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::TextUnformatted(TranslateText("Remove all recent NPCs from this save?"));
            if (ImGuiMCP::Button(TranslateText("Clear"))) {
                savedNpcs_.ClearRecent();
                ResetListPaginationForQuery(recentListPagination_);
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(TranslateText("Cancel"))) ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
        }
        if (selected_) {
            ImGuiMCP::Separator();
            RenderDetails(true);
        }
        }
        ImGuiMCP::EndChild();
        RenderRootModals();
    }
}
