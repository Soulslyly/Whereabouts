#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "Search/RuntimeIndex.h"
#include "UI/Menu.h"
#include "UI/MenuModel.h"

#include <algorithm>
#include <array>
#include <format>

namespace whereabouts::ui
{
    namespace
    {
        enum class LocationColumn : std::uint32_t
        {
            Name = 1,
            Plugin = 2,
            Worldspace = 3
        };

        std::string LocalizedLocationCopyConfirmation(
            const LocationCopySelection& selection) noexcept
        {
            if (selection.value.empty()) {
                return TranslateOwned("No ID is available for this location.");
            }
            const auto requested = TranslateOwned(LocationCopyLabel(selection.requested));
            const auto copied = TranslateOwned(LocationCopyLabel(selection.copied));
            return selection.usedFallback ?
                TranslateFormat("No {} - copied {} {}.", requested, copied, selection.value) :
                TranslateFormat("Copied {} {}.", copied, selection.value);
        }
    }

    void Menu::RenderLocations()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        if (const auto refresh = locationSearchRefreshState_.Consume()) {
            RunLocationSearch(*refresh);
        }

        ImGuiMCP::SeparatorText(TranslateText("Search locations"));
        ImGuiMCP::Spacing();
        const bool wideControls = UseWideLocationControls(ImGuiMCP::GetContentRegionAvail().x);
        ImGuiMCP::SetNextItemWidth(wideControls ? 420.0F : -1.0F);
        const auto label = std::format(
            "{}##WhereaboutsLocationSearch", TranslateText("Location name or ID"));
        const bool submitted = ImGuiMCP::InputText(
            label.c_str(),
            locationSearchText_.data(),
            locationSearchText_.size(),
            ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
        const bool edited = ImGuiMCP::IsItemEdited();
        DelayedTooltip(TranslateText(
            "Search by cell name, EditorID, FormID, plugin, location, or worldspace."));
        if (submitted) RunLocationSearch(SearchRun::Submitted);
        else if (edited && settings_.liveSearch) RunLocationSearch(SearchRun::Preview);
        if (wideControls) ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Search"))) {
            RunLocationSearch(SearchRun::Submitted);
        }

        const std::array sortNames{
            TranslateText("Name"), TranslateText("Plugin"), TranslateText("Worldspace")};
        if (wideControls) ImGuiMCP::SameLine();
        ImGuiMCP::SetNextItemWidth(155.0F);
        if (ImGuiMCP::BeginCombo(
                TranslateText("Sort by"),
                sortNames[std::clamp(locationSortIndex_, 0, 2)])) {
            for (int index = 0; index < static_cast<int>(sortNames.size()); ++index) {
                if (ImGuiMCP::Selectable(sortNames[index], locationSortIndex_ == index)) {
                    locationSortIndex_ = index;
                    RunLocationSearch(locationSearchRefreshState_.Current());
                }
            }
            ImGuiMCP::EndCombo();
        }
        if (wideControls) ImGuiMCP::SameLine();
        if (ImGuiMCP::Checkbox(TranslateText("Ascending"), &locationAscending_)) {
            RunLocationSearch(locationSearchRefreshState_.Current());
        }

        ImGuiMCP::Spacing();
        const auto available = ImGuiMCP::GetContentRegionAvail();
        const auto layout = ChoosePaneLayout(available.x);
        constexpr float spacing = 8.0F;
        const float resultWidth = layout == PaneLayout::SideBySide ?
            (available.x - spacing) * std::clamp(locationPaneRatio_, 0.3F, 0.7F) : available.x;
        const float resultHeight = layout == PaneLayout::Stacked ?
            ComputeStackedPaneHeight(available.y, locationPaneRatio_) : 0.0F;

        if (ImGuiMCP::BeginChild(
                "##WhereaboutsLocationResults",
                {resultWidth, resultHeight},
                ImGuiMCP::ImGuiChildFlags_Border)) {
            const auto summary = TranslateFormat(
                "{} shown / {} matches", locationResults_.size(), locationResultTotal_);
            ImGuiMCP::TextUnformatted(summary.c_str());
            ImGuiMCP::Separator();
            const float footerReserve = ImGuiMCP::GetFrameHeightWithSpacing() * 1.35F;
            bool moreRowsBelow = false;
            if (ImGuiMCP::BeginChild("##WhereaboutsLocationRows", {0.0F, -footerReserve})) {
                const auto pushedColors = PushThemeSafeRowColors();
                if (ImGuiMCP::BeginTable(
                        "##WhereaboutsLocationTable",
                        3,
                        ImGuiMCP::ImGuiTableFlags_RowBg |
                            ImGuiMCP::ImGuiTableFlags_BordersInnerH |
                            ImGuiMCP::ImGuiTableFlags_SizingStretchProp |
                            ImGuiMCP::ImGuiTableFlags_Resizable |
                            ImGuiMCP::ImGuiTableFlags_Sortable)) {
                    ImGuiMCP::TableSetupColumn(
                        TranslateText("Location"),
                        ImGuiMCP::ImGuiTableColumnFlags_DefaultSort,
                        0.46F,
                        static_cast<std::uint32_t>(LocationColumn::Name));
                    ImGuiMCP::TableSetupColumn(
                        TranslateText("Plugin"),
                        ImGuiMCP::ImGuiTableColumnFlags_None,
                        0.30F,
                        static_cast<std::uint32_t>(LocationColumn::Plugin));
                    ImGuiMCP::TableSetupColumn(
                        TranslateText("Worldspace"),
                        ImGuiMCP::ImGuiTableColumnFlags_None,
                        0.24F,
                        static_cast<std::uint32_t>(LocationColumn::Worldspace));
                    ImGuiMCP::TableHeadersRow();
                    if (auto* specs = ImGuiMCP::TableGetSortSpecs(); specs && specs->SpecsDirty) {
                        if (specs->SpecsCount > 0) {
                            const auto& spec = specs->Specs[0];
                            locationSortIndex_ = spec.ColumnUserID ==
                                static_cast<std::uint32_t>(LocationColumn::Plugin) ? 1 :
                                spec.ColumnUserID ==
                                static_cast<std::uint32_t>(LocationColumn::Worldspace) ? 2 : 0;
                            locationAscending_ = spec.SortDirection !=
                                ImGuiMCP::ImGuiSortDirection_Descending;
                            RunLocationSearch(locationSearchRefreshState_.Current());
                        }
                        specs->SpecsDirty = false;
                    }
                    const bool showSecondary = ShowsSecondaryResultMetadata(settings_.resultDensity);
                    const float rowHeight = ImGuiMCP::GetTextLineHeightWithSpacing() *
                        (showSecondary ? 2.0F : 1.0F);
                    for (const auto& location : locationResults_) {
                        ImGuiMCP::PushID(static_cast<int>(location.runtimeFormID));
                        ImGuiMCP::TableNextRow(0, rowHeight);
                        RowInteraction interaction;
                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
                        const auto nameHit = BeginRowInteractionCell("##locationName", rowHeight);
                        interaction.Include(nameHit.hovered, nameHit.activated);
                        const auto nameWidth = ImGuiMCP::GetContentRegionAvail().x;
                        ImGuiMCP::TextUnformatted(location.displayName.c_str());
                        const std::string editorID = location.editorID.empty() ? "-" : location.editorID;
                        if (showSecondary) {
                            OverflowTooltip(location.displayName, nameWidth);
                            const auto editorWidth = ImGuiMCP::GetContentRegionAvail().x;
                            ImGuiMCP::TextColored(
                                *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                                "%s", editorID.c_str());
                            OverflowTooltip(editorID, editorWidth);
                        } else {
                            const auto tooltip = std::format(
                                "{}\n{}", location.displayName, editorID);
                            DelayedTooltip(tooltip.c_str());
                        }

                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                        const auto pluginHit = BeginRowInteractionCell("##locationPlugin", rowHeight);
                        interaction.Include(pluginHit.hovered, pluginHit.activated);
                        const std::string plugin{location.SourcePlugin()};
                        const auto pluginWidth = ImGuiMCP::GetContentRegionAvail().x;
                        ImGuiMCP::TextUnformatted(plugin.c_str());
                        const auto formID = std::format("{:08X}", location.runtimeFormID);
                        if (showSecondary) {
                            OverflowTooltip(plugin, pluginWidth);
                            const auto formIDWidth = ImGuiMCP::GetContentRegionAvail().x;
                            ImGuiMCP::TextColored(
                                *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                                "%s", formID.c_str());
                            OverflowTooltip(formID, formIDWidth);
                        } else {
                            const auto tooltip = std::format("{}\n{}", plugin, formID);
                            DelayedTooltip(tooltip.c_str());
                        }

                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
                        const auto worldHit = BeginRowInteractionCell("##locationWorld", rowHeight);
                        interaction.Include(worldHit.hovered, worldHit.activated);
                        const std::string worldspace = location.worldspace.empty() ?
                            TranslateOwned("Interior") : location.worldspace;
                        const auto worldspaceWidth = ImGuiMCP::GetContentRegionAvail().x;
                        ImGuiMCP::TextUnformatted(worldspace.c_str());
                        if (showSecondary && !location.containingLocation.empty()) {
                            OverflowTooltip(worldspace, worldspaceWidth);
                            const auto containingWidth = ImGuiMCP::GetContentRegionAvail().x;
                            ImGuiMCP::TextColored(
                                *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                                "%s", location.containingLocation.c_str());
                            OverflowTooltip(location.containingLocation, containingWidth);
                        } else if (!showSecondary) {
                            const auto tooltip = location.containingLocation.empty() ? worldspace :
                                std::format("{}\n{}", worldspace, location.containingLocation);
                            DelayedTooltip(tooltip.c_str());
                        }
                        const bool selected = selectedLocation_ &&
                            selectedLocation_->runtimeFormID == location.runtimeFormID;
                        ApplyUnifiedRowBackground(interaction, selected);
                        if (interaction.activated) SelectLocation(location);
                        ImGuiMCP::PopID();
                    }
                    ImGuiMCP::EndTable();
                }
                if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
                moreRowsBelow = ImGuiMCP::GetScrollY() + 1.0F < ImGuiMCP::GetScrollMaxY();
            }
            ImGuiMCP::EndChild();

            const auto footer = SelectResultFooter(
                locationSearchRefreshState_.Current(),
                locationResults_.size(),
                locationResultTotal_,
                moreRowsBelow);
            if (footer == ResultFooter::ExpandPreview) {
                const auto labelMore = TranslateFormat(
                    "Show {} more - click or press Enter",
                    RemainingResultCount(locationResults_.size(), locationResultTotal_));
                if (ImGuiMCP::Button(labelMore.c_str(), {-1.0F, 0.0F})) {
                    RunLocationSearch(SearchRun::Submitted);
                }
            } else if (footer == ResultFooter::RefineSearch) {
                const auto refine = TranslateFormat(
                    "{} shown / refine your search to see the remaining {}",
                    locationResults_.size(),
                    RemainingResultCount(locationResults_.size(), locationResultTotal_));
                ImGuiMCP::TextUnformatted(refine.c_str());
            }
        }
        ImGuiMCP::EndChild();

        if (layout == PaneLayout::SideBySide) ImGuiMCP::SameLine(0.0F, spacing);
        if (ImGuiMCP::BeginChild(
                "##WhereaboutsLocationDetails",
                {0.0F, 0.0F},
                ImGuiMCP::ImGuiChildFlags_Border)) {
            if (selectedLocation_) RenderLocationDetails();
            else ImGuiMCP::TextUnformatted(TranslateText("No location selected."));
        }
        ImGuiMCP::EndChild();
        RenderRootModals();
    }

    void Menu::RenderLocationDetails()
    {
        ImGuiMCP::SeparatorText(TranslateText("Selected Location"));
        if (!selectedLocation_) {
            ImGuiMCP::TextUnformatted(TranslateText("No location selected."));
            return;
        }

        const auto& location = *selectedLocation_;
        ImGuiMCP::Spacing();
        ImGuiMCP::TextUnformatted(location.displayName.c_str());
        ImGuiMCP::Spacing();
        if (ImGuiMCP::BeginTable(
                "##WhereaboutsLocationDetailTable",
                2,
                ImGuiMCP::ImGuiTableFlags_SizingStretchProp |
                    ImGuiMCP::ImGuiTableFlags_RowBg)) {
            ImGuiMCP::TableSetupColumn(
                "##WhereaboutsLocationDetailLabel",
                ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
                175.0F);
            ImGuiMCP::TableSetupColumn(
                "##WhereaboutsLocationDetailValue",
                ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            const auto detail = [](const char* label, const std::string& value) {
                ImGuiMCP::TableNextRow();
                static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
                ImGuiMCP::TextUnformatted(TranslateText(label));
                static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                ImGuiMCP::TextWrapped("%s", value.empty() ? "-" : value.c_str());
            };
            detail("Cell", location.displayName);
            detail("EditorID", location.editorID);
            detail("Plugin", std::string{location.SourcePlugin()});
            detail("FormID", std::format("{:08X}", location.runtimeFormID));
            detail("Location record", location.containingLocation);
            detail("Worldspace", location.worldspace);
            detail("Type", TranslateOwned(location.interior ? "Interior" : "Exterior"));
            if (location.hasExteriorGrid) {
                detail("Exterior grid", std::format(
                    "{}, {}", location.exteriorCellX, location.exteriorCellY));
            }
            ImGuiMCP::EndTable();
        }
        ImGuiMCP::Spacing();
        ImGuiMCP::SetNextItemWidth(-1.0F);
        if (ImGuiMCP::BeginCombo("##WhereaboutsLocationCopy", TranslateText("Copy"))) {
            constexpr std::array copyKinds{
                LocationCopyKind::FormID,
                LocationCopyKind::EditorID,
                LocationCopyKind::Stable,
                LocationCopyKind::CocCommand};
            for (const auto kind : copyKinds) {
                const auto label = TranslateOwned(LocationCopyLabel(kind));
                if (ImGuiMCP::Selectable(label.c_str())) {
                    const auto selection = ResolveLocationCopy(location, kind);
                    if (!selection.value.empty()) {
                        ImGuiMCP::SetClipboardText(selection.value.c_str());
                    }
                    SetLocationStatus(LocalizedLocationCopyConfirmation(selection));
                }
            }
            ImGuiMCP::EndCombo();
        }
        DelayedTooltip(TranslateText("Choose which location value to copy."));
        if (ImGuiMCP::Button(TranslateText("Travel to Location"), {-1.0F, 0.0F})) {
            RequestLocationTravel();
        }
        DelayedTooltip(TranslateText("Closes the menu, then asks Skyrim to load this cell."));
        const auto status = LocationStatus();
        if (!status.empty()) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextWrapped("%s", status.c_str());
        }
    }
}
