#include "PCH.h"

#include "Core/SpatialPresentation.h"

#include "SKSEMenuFramework.h"
#include "Search/RuntimeIndex.h"
#include "UI/Menu.h"
#include "UI/FilterLayoutModel.h"
#include "UI/MenuModel.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>

namespace whereabouts::ui
{
    void Menu::RenderSearchLocationResults()
    {
        if (!IncludesLocations(searchContent_) || searchLocationResults_.empty()) return;
        if (searchContent_ == SearchContent::NpcsAndLocations) {
            ImGuiMCP::Spacing();
            ImGuiMCP::SeparatorText(TranslateText("Locations"));
        }
        if (!ImGuiMCP::BeginTable(
                "##WhereaboutsSearchLocationTable",
                3,
                ImGuiMCP::ImGuiTableFlags_RowBg |
                    ImGuiMCP::ImGuiTableFlags_BordersInnerH |
                    ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
            return;
        }
        ImGuiMCP::TableSetupColumn(TranslateText("Location"), 0, 0.46F);
        ImGuiMCP::TableSetupColumn(TranslateText("Plugin"), 0, 0.30F);
        ImGuiMCP::TableSetupColumn(TranslateText("Worldspace"), 0, 0.24F);
        ImGuiMCP::TableHeadersRow();
        const bool showSecondary = ShowsSecondaryResultMetadata(
            settings_.DensityFor(UiDensityArea::Results));
        const float rowHeight = ImGuiMCP::GetTextLineHeightWithSpacing() *
            (showSecondary ? 2.0F : 1.0F);
        for (const auto& location : searchLocationResults_) {
            ImGuiMCP::PushID(static_cast<int>(location.runtimeFormID));
            ImGuiMCP::TableNextRow(0, rowHeight);
            RowInteraction interaction;
            static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
            const auto nameHit = BeginRowInteractionCell("##searchPlaceName", rowHeight);
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
                const auto tooltip = std::format("{}\n{}", location.displayName, editorID);
                DelayedTooltip(tooltip.c_str());
            }

            static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
            const auto pluginHit = BeginRowInteractionCell("##searchPlacePlugin", rowHeight);
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
            const auto worldHit = BeginRowInteractionCell("##searchPlaceWorld", rowHeight);
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

    void Menu::RenderSearch()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        if (const auto refresh = searchRefreshState_.Consume()) RunSearch(*refresh);
        SyncSelectedTarget();
        const auto controlWidth = ImGuiMCP::GetContentRegionAvail().x;
        const bool wideControls = controlWidth >= 900.0F;
        const auto searchDensity = settings_.DensityFor(UiDensityArea::Search);
        const auto* searchStyle = ImGuiMCP::GetStyle();
        int searchStyleVars = 0;
        if (searchDensity != ResultDensity::Detailed && searchStyle) {
            const float scale = searchDensity == ResultDensity::Compact ? 0.78F : 0.58F;
            ImGuiMCP::PushStyleVar(
                ImGuiMCP::ImGuiStyleVar_FramePadding,
                {searchStyle->FramePadding.x,
                 (std::max)(1.0F, searchStyle->FramePadding.y * scale)});
            ImGuiMCP::PushStyleVar(
                ImGuiMCP::ImGuiStyleVar_ItemSpacing,
                {searchStyle->ItemSpacing.x,
                 (std::max)(1.0F, searchStyle->ItemSpacing.y * scale)});
            searchStyleVars = 2;
        }

        ImGuiMCP::SeparatorText(TranslateText("Search and select"));
        ImGuiMCP::Spacing();
        ImGuiMCP::SetNextItemWidth(-1.0F);
        const auto searchPrompt = searchContent_ == SearchContent::LocationsOnly ?
            "Location name or ID" :
            searchContent_ == SearchContent::NpcsAndLocations ?
                "NPC or location name or ID" : "NPC name or ID";
        const auto searchLabel = std::format(
            "{}##WhereaboutsSearch",
            TranslateText(searchPrompt));
        const bool submitted = ImGuiMCP::InputText(
            searchLabel.c_str(),
            searchText_.data(),
            searchText_.size(),
            ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
        DelayedTooltip(TranslateText(
            "Search by name, EditorID, FormID, or stable plugin ID."));
        const bool searchEdited = ImGuiMCP::IsItemEdited();
        if (submitted) {
            RunSearch(SearchRun::Submitted, true);
        } else if (searchEdited && settings_.liveSearch) {
            RunSearch(SearchRun::Preview);
        }
        bool searchOptionsChanged = false;
        bool catalogModeChanged = false;
        const auto toolbarColumns = SearchToolbarColumns(controlWidth);
        if (ImGuiMCP::BeginTable(
                "##WhereaboutsSearchToolbar",
                toolbarColumns,
                ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
            static_cast<void>(ImGuiMCP::TableNextColumn());
            if (ImGuiMCP::Button(TranslateText("Search"), {-1.0F, 0.0F})) {
                RunSearch(SearchRun::Submitted, true);
            }
            static_cast<void>(ImGuiMCP::TableNextColumn());
            if (ImGuiMCP::Button(TranslateText("Virtual Keyboard"), {-1.0F, 0.0F})) {
                controllerBackup_ = searchText_.data();
                controllerResultsBackup_ = results_;
                controllerLocationResultsBackup_ = searchLocationResults_;
                controllerSuggestionsBackup_ = searchSuggestions_;
                controllerResultTotalBackup_ = resultTotal_;
                controllerResultTextMatchTotalBackup_ = resultTextMatchTotal_;
                controllerLocationResultTotalBackup_ = searchLocationResultTotal_;
                controllerSearchErrorBackup_ = searchError_;
                controllerSearchRunBackup_ = searchRefreshState_.Current();
                openControllerKeyboard_ = true;
            }
            static_cast<void>(ImGuiMCP::TableNextColumn());
            if (ImGuiMCP::Button(TranslateText("Use Console Target"), {-1.0F, 0.0F})) {
                QueueUseConsoleTarget();
            }
            static_cast<void>(ImGuiMCP::TableNextColumn());
            if (ImGuiMCP::Button(TranslateText("Use Crosshair Target"), {-1.0F, 0.0F})) {
                QueueUseCrosshairTarget();
            }
            static_cast<void>(ImGuiMCP::TableNextColumn());
            ImGuiMCP::BeginDisabled(!selected_ && !selectedLocation_);
            if (ImGuiMCP::Button(TranslateText("Clear Selection"), {-1.0F, 0.0F})) ClearSelection();
            ImGuiMCP::EndDisabled();
            ImGuiMCP::EndTable();
        }
        if (searchStyleVars > 0) ImGuiMCP::PopStyleVar(searchStyleVars);
        if (!searchError_.empty()) {
            const auto message = TranslateFormat("Search error: {}", searchError_);
            ImGuiMCP::TextWrapped("%s", message.c_str());
        }
        if (!searchSuggestions_.empty()) {
            ImGuiMCP::TextUnformatted(TranslateText("Did you mean:"));
            ImGuiMCP::SameLine();
            std::optional<std::string> acceptedSuggestion;
            for (std::size_t index = 0; index < searchSuggestions_.size(); ++index) {
                if (index > 0) ImGuiMCP::SameLine();
                const auto& suggestion = searchSuggestions_[index];
                if (ImGuiMCP::SmallButton(suggestion.c_str())) {
                    acceptedSuggestion = suggestion;
                    break;
                }
            }
            if (acceptedSuggestion) {
                searchText_.fill('\0');
                const auto length = (std::min)(
                    acceptedSuggestion->size(), searchText_.size() - 1);
                std::copy_n(acceptedSuggestion->data(), length, searchText_.data());
                RunSearch(SearchRun::Submitted, true);
            }
        }
        ImGuiMCP::Spacing();
        RenderSearchFilters(searchOptionsChanged, catalogModeChanged);

        if (catalogModeChanged) {
            RunSearch(searchRefreshState_.Current());
        } else if (searchOptionsChanged && settings_.liveSearch) {
            RunSearch(SearchRun::Preview);
        }
        ImGuiMCP::Spacing();
        const auto available = ImGuiMCP::GetContentRegionAvail();
        const auto layout = ChoosePaneLayout(available.x);
        constexpr float splitterThickness = 6.0F;
        const auto* style = ImGuiMCP::GetStyle();
        const float horizontalSpacing = style ? style->ItemSpacing.x : 8.0F;
        const float verticalSpacing = style ? style->ItemSpacing.y : 4.0F;
        searchPaneRatio_ = ClampSearchPaneRatio(searchPaneRatio_);
        const float usableWidth = (std::max)(
            1.0F, available.x - splitterThickness - horizontalSpacing * 2.0F);
        const float usableHeight = (std::max)(
            1.0F, available.y - splitterThickness - verticalSpacing * 2.0F);
        const float leftWidth = layout == PaneLayout::SideBySide ?
            usableWidth * searchPaneRatio_ : available.x;
        float paneHeight = 0.0F;
        if (layout == PaneLayout::Stacked) {
            paneHeight = usableHeight * searchPaneRatio_;
            if (usableHeight >= 360.0F) paneHeight = std::clamp(paneHeight, 180.0F, usableHeight - 180.0F);
        }

        if (ImGuiMCP::BeginChild(
                "##WhereaboutsResults",
                {leftWidth, paneHeight},
                ImGuiMCP::ImGuiChildFlags_Border)) {
            const auto indexView = index_.Snapshot();
            const auto indexState = indexView ?
                ClassifyIndexUi(
                    indexView->readiness,
                    indexView->failure,
                    (IncludesNpcs(searchContent_) && !indexView->catalog->empty()) ||
                        (IncludesLocations(searchContent_) && !indexView->locations->empty())) :
                IndexUiState::Preparing;
            if (indexState == IndexUiState::Preparing) {
                ImGuiMCP::TextUnformatted(TranslateText("Preparing search index..."));
            } else if (indexState == IndexUiState::FailedEmpty) {
                const auto message = IndexFailureText(
                    indexView ? indexView->failure : IndexFailure::BuildFailed);
                const auto localized = TranslateOwned(message);
                ImGuiMCP::TextWrapped("%s", localized.c_str());
            } else {
            if (indexState == IndexUiState::FailedWithCatalog) {
                const auto message = IndexFailureText(indexView->failure);
                const auto localized = TranslateOwned(message);
                ImGuiMCP::TextWrapped("%s", localized.c_str());
                ImGuiMCP::Separator();
            }
            const auto summary = TranslateFormat(
                "{} shown / {} matches",
                results_.size() + searchLocationResults_.size(),
                resultTotal_ + searchLocationResultTotal_);
            ImGuiMCP::TextUnformatted(summary.c_str());
            ImGuiMCP::Separator();

            const auto recovery = ClassifySearchMissRecovery(
                searchRefreshState_.Current(),
                !searchError_.empty(),
                resultTextMatchTotal_,
                resultTotal_,
                searchLocationResultTotal_,
                ActiveSearchFilterCount(BuildSearchFilters()));
            if (recovery == SearchMissRecovery::ClearFilters) {
                ImGuiMCP::TextWrapped("%s", TranslateText(
                    "NPC matches were found but hidden by the current filters."));
                if (ImGuiMCP::Button(
                        TranslateText("Clear Filters and Search Again"), {-1.0F, 0.0F})) {
                    ResetFiltersToDefaults();
                    RunSearch(SearchRun::Submitted, true);
                }
                ImGuiMCP::Spacing();
            } else if (recovery == SearchMissRecovery::RefreshIndex) {
                ImGuiMCP::TextWrapped("%s", TranslateText(
                    "No matching NPC or location is in the current search index."));
                ImGuiMCP::BeginDisabled(manualRefreshState_ == ManualRefreshState::Waiting);
                if (ImGuiMCP::Button(
                        TranslateText("Refresh Index and Search Again"), {-1.0F, 0.0F})) {
                    searchRefreshState_.Select(SearchRun::Submitted);
                    searchAfterIndexRefresh_ = true;
                    QueueIndexRefresh();
                }
                ImGuiMCP::EndDisabled();
                ImGuiMCP::Spacing();
            }

            const auto footer = SelectResultFooter(
                searchRefreshState_.Current(),
                results_.size() + searchLocationResults_.size(),
                resultTotal_ + searchLocationResultTotal_);
            const float resultRowsHeight = footer == ResultFooter::None ? 0.0F :
                -ImGuiMCP::GetFrameHeightWithSpacing() * 1.35F;
            if (ImGuiMCP::BeginChild(
                    "##WhereaboutsResultRows",
                    {0.0F, resultRowsHeight})) {
                const auto pushedColors = PushThemeSafeRowColors();
                const bool locationsFirst = LocationsAppearFirst(
                    searchContent_, resultSectionOrder_);
                if (locationsFirst) RenderSearchLocationResults();
                const auto resultDensity = settings_.DensityFor(UiDensityArea::Results);
                const bool superCompact = IsSuperCompactResultDensity(resultDensity);
                const bool showQuickAction = settings_.rowButtonAction != kDisabledActionID;
                if (IncludesNpcs(searchContent_) && ImGuiMCP::BeginTable(
                        superCompact ? "##WhereaboutsSuperCompactSearchResultTable" :
                                       "##WhereaboutsSearchResultTable",
                        (superCompact ? 2 : 3) + (showQuickAction ? 1 : 0),
                        ImGuiMCP::ImGuiTableFlags_RowBg |
                            ImGuiMCP::ImGuiTableFlags_BordersInnerH |
                            ImGuiMCP::ImGuiTableFlags_SizingStretchProp |
                            ImGuiMCP::ImGuiTableFlags_Resizable |
                            ImGuiMCP::ImGuiTableFlags_Reorderable |
                            ImGuiMCP::ImGuiTableFlags_Sortable |
                            ImGuiMCP::ImGuiTableFlags_SortTristate)) {
                    ImGuiMCP::TableSetupColumn(
                        TranslateText("NPC"),
                        ImGuiMCP::ImGuiTableColumnFlags_WidthStretch |
                            ImGuiMCP::ImGuiTableColumnFlags_DefaultSort,
                        superCompact ? 0.72F : 0.48F,
                        static_cast<std::uint32_t>(SearchColumnId::Npc));
                    if (superCompact) {
                        ImGuiMCP::TableSetupColumn(
                            TranslateText("FormID"),
                            ImGuiMCP::ImGuiTableColumnFlags_WidthFixed |
                                ImGuiMCP::ImGuiTableColumnFlags_NoSort,
                            110.0F);
                    } else {
                        ImGuiMCP::TableSetupColumn(
                            TranslateText("Status"),
                            ImGuiMCP::ImGuiTableColumnFlags_WidthStretch,
                            0.14F,
                            static_cast<std::uint32_t>(SearchColumnId::Status));
                        ImGuiMCP::TableSetupColumn(
                            TranslateText("Location"),
                            ImGuiMCP::ImGuiTableColumnFlags_WidthStretch,
                            0.38F,
                            static_cast<std::uint32_t>(SearchColumnId::Location));
                    }
                    if (showQuickAction) {
                        ImGuiMCP::TableSetupColumn(
                            TranslateText("Action"),
                            ImGuiMCP::ImGuiTableColumnFlags_WidthFixed |
                                ImGuiMCP::ImGuiTableColumnFlags_NoSort,
                            resultDensity == ResultDensity::Detailed ? 112.0F : 56.0F);
                    }
                    ImGuiMCP::TableHeadersRow();
                    if (searchSortUiDirty_) {
                        ImGuiMCP::TableSetColumnSortDirection(
                            0, ImGuiMCP::ImGuiSortDirection_None, false);
                        if (!superCompact) {
                            ImGuiMCP::TableSetColumnSortDirection(
                                1, ImGuiMCP::ImGuiSortDirection_None, false);
                            ImGuiMCP::TableSetColumnSortDirection(
                                2, ImGuiMCP::ImGuiSortDirection_None, false);
                        }
                        const auto direction = ascending_ ?
                            ImGuiMCP::ImGuiSortDirection_Ascending :
                            ImGuiMCP::ImGuiSortDirection_Descending;
                        if (sortIndex_ == static_cast<int>(SortKey::Name)) {
                            ImGuiMCP::TableSetColumnSortDirection(0, direction, false);
                        } else if (!superCompact &&
                                   sortIndex_ == static_cast<int>(SortKey::Location)) {
                            ImGuiMCP::TableSetColumnSortDirection(2, direction, false);
                        } else if (!superCompact &&
                                   sortIndex_ == static_cast<int>(SortKey::Status)) {
                            ImGuiMCP::TableSetColumnSortDirection(1, direction, false);
                        }
                        searchSortUiDirty_ = false;
                        if (auto* specs = ImGuiMCP::TableGetSortSpecs()) specs->SpecsDirty = false;
                    } else if (auto* specs = ImGuiMCP::TableGetSortSpecs(); specs && specs->SpecsDirty) {
                        if (specs->SpecsCount > 0) {
                            const auto& spec = specs->Specs[0];
                            const auto headerSort = SearchHeaderSort(
                                spec.ColumnUserID,
                                spec.SortDirection != ImGuiMCP::ImGuiSortDirection_Descending);
                            if (headerSort &&
                                (sortIndex_ != static_cast<int>(headerSort->first) ||
                                 ascending_ != headerSort->second)) {
                                sortIndex_ = static_cast<int>(headerSort->first);
                                ascending_ = headerSort->second;
                                RunSearch(searchRefreshState_.Current());
                            }
                        } else {
                            sortIndex_ = static_cast<int>(SortKey::Name);
                            ascending_ = true;
                            ImGuiMCP::TableSetColumnSortDirection(
                                0, ImGuiMCP::ImGuiSortDirection_Ascending, false);
                            RunSearch(searchRefreshState_.Current());
                        }
                        specs->SpecsDirty = false;
                    }
                    const bool showSecondary = ShowsSecondaryResultMetadata(resultDensity);
                    const float rowHeight = ImGuiMCP::GetTextLineHeightWithSpacing() *
                        (showSecondary ? 2.0F : 1.0F);
                    const auto favoriteEntries = savedNpcs_.Favorites();
                    for (const auto& npc : results_) {
                        ImGuiMCP::PushID(static_cast<int>(npc.ReferenceRuntimeID()));
                        ImGuiMCP::TableNextRow(0, rowHeight);
                        RowInteraction interaction;
                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
                        const auto nameHit = BeginRowInteractionCell("##searchNpcCell", rowHeight);
                        interaction.Include(nameHit.hovered, nameHit.activated);
                        const auto nameWidth = ImGuiMCP::GetContentRegionAvail().x;
                        const bool selected = selected_ &&
                            selected_->ReferenceRuntimeID() == npc.ReferenceRuntimeID();
                        ImGuiMCP::TextUnformatted(npc.displayName.c_str());
                        if (superCompact) {
                            OverflowTooltip(npc.displayName, nameWidth);
                            static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                            const auto formIdHit = BeginRowInteractionCell(
                                "##searchFormIdCell", rowHeight);
                            interaction.Include(formIdHit.hovered, formIdHit.activated);
                            ImGuiMCP::Text("%08X", npc.ReferenceRuntimeID());
                            if (showQuickAction) {
                                static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
                                RenderQuickActionButton(npc, TargetSource::Search);
                            }
                            ApplyUnifiedRowBackground(interaction, selected);
                            HandleNpcRowActivation(interaction, npc, TargetSource::Search);
                            ImGuiMCP::PopID();
                            continue;
                        }
                        const auto identity = std::format(
                            "{} / {:08X}",
                            npc.SourcePlugin().empty() ? std::string_view{"Dynamic"} : npc.SourcePlugin(),
                            npc.ReferenceRuntimeID());
                        if (showSecondary) {
                            OverflowTooltip(npc.displayName, nameWidth);
                            const auto identityWidth = ImGuiMCP::GetContentRegionAvail().x;
                            ImGuiMCP::TextColored(
                                *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                                "%s",
                                identity.c_str());
                            OverflowTooltip(identity, identityWidth);
                        } else {
                            const auto tooltip = std::format("{}\n{}", npc.displayName, identity);
                            DelayedTooltip(tooltip.c_str());
                        }

                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                        const auto statusHit = BeginRowInteractionCell("##searchStatusCell", rowHeight);
                        interaction.Include(statusHit.hovered, statusHit.activated);
                        const bool favorite = npc.StableReference().IsPersistable() &&
                            std::ranges::any_of(favoriteEntries, [&](const auto& entry) {
                                return entry.identity == npc.StableReference();
                            });
                        RenderDenseStatusBadges(npc, false, selected, favorite);

                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
                        const auto locationHit = BeginRowInteractionCell("##searchLocationCell", rowHeight);
                        interaction.Include(locationHit.hovered, locationHit.activated);
                        const auto location = LocalizedPrimarySpatialLabel(npc.spatial);
                        const auto locationWidth = ImGuiMCP::GetContentRegionAvail().x;
                        ImGuiMCP::TextUnformatted(location.c_str());
                        const auto worldspace = SecondaryWorldspaceLabel(npc.spatial);
                        if (showSecondary && !worldspace.empty()) {
                            OverflowTooltip(location, locationWidth);
                            const auto worldspaceWidth = ImGuiMCP::GetContentRegionAvail().x;
                            ImGuiMCP::PushStyleColor(
                                ImGuiMCP::ImGuiCol_Text,
                                *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled));
                            ImGuiMCP::TextUnformatted(worldspace.c_str());
                            ImGuiMCP::PopStyleColor();
                            OverflowTooltip(worldspace, worldspaceWidth);
                        } else if (!showSecondary) {
                            const auto tooltip = worldspace.empty() ? location :
                                std::format("{}\n{}", location, worldspace);
                            DelayedTooltip(tooltip.c_str());
                        }
                        if (showQuickAction) {
                            static_cast<void>(ImGuiMCP::TableSetColumnIndex(3));
                            RenderQuickActionButton(npc, TargetSource::Search);
                        }
                        ApplyUnifiedRowBackground(interaction, selected);
                        HandleNpcRowActivation(interaction, npc, TargetSource::Search);
                        ImGuiMCP::PopID();
                    }
                    ImGuiMCP::EndTable();
                }
                if (!locationsFirst) RenderSearchLocationResults();
                if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
            }
            ImGuiMCP::EndChild();

            if (footer == ResultFooter::ExpandPreview) {
                const auto remaining = RemainingResultCount(
                    results_.size() + searchLocationResults_.size(),
                    resultTotal_ + searchLocationResultTotal_);
                const auto label = TranslateFormat(
                    "Show {} more - click", remaining);
                if (ImGuiMCP::Button(label.c_str(), {-1.0F, 0.0F})) {
                    RunSearch(SearchRun::Submitted);
                }
            } else if (footer == ResultFooter::RefineSearch) {
                const auto label = TranslateFormat(
                    "{} shown / refine your search to see the remaining {}",
                    results_.size() + searchLocationResults_.size(),
                    RemainingResultCount(
                        results_.size() + searchLocationResults_.size(),
                        resultTotal_ + searchLocationResultTotal_));
                ImGuiMCP::TextUnformatted(label.c_str());
            }
            }
        }
        ImGuiMCP::EndChild();

        if (layout == PaneLayout::SideBySide) {
            ImGuiMCP::SameLine();
            static_cast<void>(ImGuiMCP::Button(
                "##WhereaboutsVerticalSplitter",
                {splitterThickness, available.y}));
            if (ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemActive()) {
                ImGuiMCP::SetMouseCursor(ImGuiMCP::ImGuiMouseCursor_ResizeEW);
            }
            if (ImGuiMCP::IsItemActive()) {
                if (const auto* io = ImGuiMCP::GetIO()) {
                    searchPaneRatio_ = ClampSearchPaneRatio(
                        searchPaneRatio_ + io->MouseDelta.x / usableWidth);
                }
            }
            DelayedTooltip(TranslateText("Drag to resize the result and detail panes."));
            ImGuiMCP::SameLine();
        } else {
            static_cast<void>(ImGuiMCP::Button(
                "##WhereaboutsHorizontalSplitter",
                {available.x, splitterThickness}));
            if (ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemActive()) {
                ImGuiMCP::SetMouseCursor(ImGuiMCP::ImGuiMouseCursor_ResizeNS);
            }
            if (ImGuiMCP::IsItemActive()) {
                if (const auto* io = ImGuiMCP::GetIO()) {
                    searchPaneRatio_ = ClampSearchPaneRatio(
                        searchPaneRatio_ + io->MouseDelta.y / usableHeight);
                }
            }
            DelayedTooltip(TranslateText("Drag to resize the result and detail panes."));
        }
        if (ImGuiMCP::BeginChild(
                "##WhereaboutsDetails",
                {0.0F, 0.0F},
                ImGuiMCP::ImGuiChildFlags_Border)) {
            RenderDetails(settings_.DensityFor(UiDensityArea::SelectedNpc));
        }
        ImGuiMCP::EndChild();
        RenderRootModals();
    }
}
