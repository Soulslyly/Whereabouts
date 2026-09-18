#include "PCH.h"

#include "Search/RuntimeIndex.h"
#include "SKSEMenuFramework.h"
#include "UI/Menu.h"

#include <algorithm>
#include <format>

namespace whereabouts::ui
{
    void Menu::RunInspectorSearch()
    {
        inspectorResults_.clear();
        const auto view = index_.Snapshot();
        if (!view || !view->catalog || inspectorSearchText_.front() == '\0') return;

        const auto indices = InspectorCandidateIndices(
            *view->catalog, inspectorSearchText_.data(), 3);
        inspectorResults_.reserve(indices.size());
        for (const auto index : indices) inspectorResults_.push_back((*view->catalog)[index]);
    }

    void Menu::RenderInspector()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        SyncSelectedTarget();

        const auto view = index_.Snapshot();
        if (view && (view->session != inspectorIndexSession_ ||
                     view->revision != inspectorIndexRevision_)) {
            inspectorIndexSession_ = view->session;
            inspectorIndexRevision_ = view->revision;
            RunInspectorSearch();
        }

        ImGuiMCP::SeparatorText(TranslateText("NPC Inspector"));
        ImGuiMCP::Spacing();
        const auto inspectorDensity = settings_.DensityFor(UiDensityArea::Inspector);
        const float availableWidth = ImGuiMCP::GetContentRegionAvail().x;
        const auto placement = ChooseInspectorSearchPlacement(availableWidth);

        const auto renderSearch = [&] {
            ImGuiMCP::SetNextItemWidth(-1.0F);
            if (ImGuiMCP::InputText(
                    "##WhereaboutsInspectorSearch",
                    inspectorSearchText_.data(),
                    inspectorSearchText_.size())) {
                RunInspectorSearch();
            }
            DelayedTooltip(TranslateText(
                "Search by NPC name, plugin, EditorID, FormID, identity, or location."));
        };

        const auto renderCandidates = [&] {
            if (inspectorResults_.empty()) {
                if (inspectorSearchText_.front() != '\0') {
                    ImGuiMCP::TextDisabled("%s", TranslateText("No matching NPCs."));
                }
                return;
            }
            const int columns = (std::min)(
                InspectorCandidateColumns(availableWidth),
                static_cast<int>(inspectorResults_.size()));
            if (!ImGuiMCP::BeginTable(
                    "##WhereaboutsInspectorCandidates",
                    (std::max)(1, columns),
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                return;
            }
            const float candidateHeight = ImGuiMCP::GetTextLineHeightWithSpacing() * 2.0F;
            for (const auto& npc : inspectorResults_) {
                static_cast<void>(ImGuiMCP::TableNextColumn());
                ImGuiMCP::PushID(static_cast<int>(npc.ReferenceRuntimeID()));
                const auto label = std::format(
                    "{}\n{:08X}###WhereaboutsInspectorCandidate",
                    npc.displayName,
                    npc.ReferenceRuntimeID());
                const bool activated = ImGuiMCP::Selectable(
                    label.c_str(),
                    selected_ && selected_->ReferenceRuntimeID() == npc.ReferenceRuntimeID(),
                    0,
                    {0.0F, candidateHeight});
                RowInteraction interaction{
                    ImGuiMCP::IsItemHovered(),
                    activated};
                const auto location = LocalizedPrimarySpatialLabel(npc.spatial);
                const auto tooltip = std::format(
                    "{}\n{:08X}\n{}\n{}",
                    npc.displayName,
                    npc.ReferenceRuntimeID(),
                    npc.SourcePlugin().empty() ? TranslateText("Dynamic / unavailable") :
                                                std::string(npc.SourcePlugin()),
                    location);
                DelayedTooltip(tooltip.c_str());
                HandleNpcRowActivation(interaction, npc, TargetSource::Inspector);
                ImGuiMCP::PopID();
            }
            ImGuiMCP::EndTable();
        };

        if (placement == InspectorSearchPlacement::BesideSearch &&
            ImGuiMCP::BeginTable(
                "##WhereaboutsInspectorSearchStrip",
                2,
                ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
            ImGuiMCP::TableSetupColumn(
                "##InspectorSearchColumn",
                ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
                (std::min)(320.0F, availableWidth * 0.32F));
            ImGuiMCP::TableSetupColumn(
                "##InspectorCandidatesColumn",
                ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            static_cast<void>(ImGuiMCP::TableNextColumn());
            renderSearch();
            static_cast<void>(ImGuiMCP::TableNextColumn());
            renderCandidates();
            ImGuiMCP::EndTable();
        } else {
            renderSearch();
            ImGuiMCP::Spacing();
            renderCandidates();
        }

        ImGuiMCP::Spacing();
        RenderDetails(inspectorDensity);
        RenderRootModals();
    }
}
