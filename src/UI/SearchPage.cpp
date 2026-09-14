#include "PCH.h"

#include "Core/SpatialPresentation.h"

#include "SKSEMenuFramework.h"
#include "Search/RuntimeIndex.h"
#include "UI/Menu.h"
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
        const bool showSecondary = ShowsSecondaryResultMetadata(settings_.resultDensity);
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
        const auto filterWidth = ImGuiMCP::GetContentRegionAvail().x;
        const auto* filterStyle = ImGuiMCP::GetStyle();
        const float filterSpacing = filterStyle ? filterStyle->ItemSpacing.x : 8.0F;
        const auto measuredControlWidth = [&](const auto& texts) {
            float width = 180.0F;
            for (const auto* text : texts) {
                width = (std::max)(
                    width,
                    ImGuiMCP::CalcTextSize(text).x +
                        ImGuiMCP::GetFrameHeightWithSpacing());
            }
            return width;
        };
        const auto drawControlLabel = [](const char* label) {
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(label);
        };
        const auto headingFilters = BuildSearchFilters();
        std::vector<FilterHeadingEntry> commonHeadingEntries;
        commonHeadingEntries.reserve(12);
        const auto addCommonHeading = [&](const char* label, std::string value) {
            commonHeadingEntries.push_back({
                label,
                TriStateFilterPreview(label, value.c_str())});
        };
        if (!headingFilters.selectedPlugins.empty()) {
            std::string plugins;
            for (const auto& plugin : headingFilters.selectedPlugins) {
                if (!plugins.empty()) plugins.append(", ");
                plugins.append(plugin);
            }
            addCommonHeading(TranslateText("Plugin"), std::move(plugins));
        } else if (headingFilters.plugin) {
            addCommonHeading(TranslateText("Plugin"), *headingFilters.plugin);
        }
        if (headingFilters.location) {
            addCommonHeading(TranslateText("Location"), *headingFilters.location);
        }
        const auto addKnownCommonHeading = [&](const char* label, std::optional<bool> value) {
            if (value) {
                addCommonHeading(
                    label,
                    TranslateOwned(*value ? "Yes" : "No"));
            }
        };
        addKnownCommonHeading(TranslateText("Alive"), headingFilters.alive);
        addKnownCommonHeading(TranslateText("Enabled"), headingFilters.enabled);
        addKnownCommonHeading(TranslateText(kFollowerFilterLabel), headingFilters.teammate);
        addKnownCommonHeading(
            TranslateText("Potential Follower"), headingFilters.potentialFollower);
        addKnownCommonHeading(TranslateText("Loaded"), headingFilters.loaded);
        if (headingFilters.favoritesOnly) {
            addCommonHeading(TranslateText("Favorites only"), TranslateOwned("Yes"));
        }
        if (headingFilters.trackedOnly) {
            addCommonHeading(TranslateText("Tracked only"), TranslateOwned("Yes"));
        }
        if (headingFilters.sameLocationOnly) {
            addCommonHeading(TranslateText("Same location"), TranslateOwned("Yes"));
        }
        if (headingFilters.includeGeneric) {
            addCommonHeading(TranslateText("Include Generic NPCs"), TranslateOwned("Yes"));
        }
        const auto commonFilterCount = ActiveCommonSearchFilterCount(headingFilters);
        const auto commonBaseHeading = TranslateOwned("Filters and sorting");
        const auto commonCountHeading = commonFilterCount == 0 ? commonBaseHeading :
            TranslateFormat("Filters and sorting ({} active)", commonFilterCount);
        const auto commonHeading = BuildFilterHeading(
            commonCountHeading,
            commonHeadingEntries,
            settings_.showActiveFilterNames,
            filterWidth,
            [&](std::string_view names) {
                return TranslateFormat(
                    "{} ({} active: {})",
                    commonBaseHeading,
                    commonFilterCount,
                    names);
            },
            [](std::string_view text) {
                return ImGuiMCP::CalcTextSize(text.data(), text.data() + text.size()).x;
            });
        const auto filterHeadingId = std::format(
            "{}###WhereaboutsFilters", commonHeading.heading);
        const bool filtersOpen = ImGuiMCP::CollapsingHeader(filterHeadingId.c_str());
        if (!commonHeading.tooltip.empty() &&
            ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_DelayNormal)) {
            ImGuiMCP::SetTooltip("%s", commonHeading.tooltip.c_str());
        }
        if (filtersOpen) {
            std::optional<std::size_t> removePluginFilter;
            float pluginRowRemaining = filterWidth;
            for (std::size_t index = 0; index < selectedPluginFilters_.size(); ++index) {
                const float chipWidth = ImGuiMCP::CalcTextSize(selectedPluginFilters_[index].c_str()).x + 48.0F;
                if (index > 0 && chipWidth <= pluginRowRemaining) ImGuiMCP::SameLine();
                else pluginRowRemaining = filterWidth;
                ImGuiMCP::PushID(static_cast<int>(index));
                if (ImGuiMCP::SmallButton("X")) removePluginFilter = index;
                DelayedTooltip(TranslateText("Remove this plugin filter."));
                ImGuiMCP::SameLine();
                const auto chipTextWidth = ImGuiMCP::GetContentRegionAvail().x;
                ImGuiMCP::TextDisabled("%s", selectedPluginFilters_[index].c_str());
                OverflowTooltip(selectedPluginFilters_[index], chipTextWidth);
                ImGuiMCP::PopID();
                pluginRowRemaining -= chipWidth;
            }
            if (removePluginFilter) {
                selectedPluginFilters_.erase(
                    selectedPluginFilters_.begin() + static_cast<std::ptrdiff_t>(*removePluginFilter));
                searchOptionsChanged = true;
            }
            const std::array contentNames{
                TranslateText("NPCs only"),
                TranslateText("NPCs and locations"),
                TranslateText("Locations only")};
            int contentIndex = static_cast<int>(searchContent_);
            const std::array primaryControlText{
                TranslateText("Plugin"), TranslateText("Location"),
                TranslateText("Search content"), TranslateText("Result order"),
                contentNames[0], contentNames[1], contentNames[2],
                TranslateText("NPCs first"), TranslateText("Locations first")};
            const int primaryControlColumns = ResponsiveControlColumns(
                filterWidth,
                measuredControlWidth(primaryControlText),
                filterSpacing,
                searchContent_ == SearchContent::NpcsAndLocations ? 4 : 3);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsPrimaryFilters",
                    primaryControlColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                static_cast<void>(ImGuiMCP::TableNextColumn());
                drawControlLabel(TranslateText("Plugin"));
                const auto pickerView = index_.Snapshot();
                std::vector<std::string> pluginNames;
                if (pickerView && pickerView->catalog) {
                    pluginNames.reserve(pickerView->catalog->size());
                    for (const auto& npc : *pickerView->catalog) {
                        if (!npc.SourcePlugin().empty()) pluginNames.emplace_back(npc.SourcePlugin());
                    }
                }
                if (pickerView && pickerView->locations && IncludesLocations(searchContent_)) {
                    for (const auto& location : *pickerView->locations) {
                        if (!location.SourcePlugin().empty()) {
                            pluginNames.emplace_back(location.SourcePlugin());
                        }
                    }
                }
                const auto pluginOptions = BuildTextPickerOptions(pluginNames);
                const auto pluginPreview = selectedPluginFilters_.empty() ?
                    TranslateOwned("Any") : selectedPluginFilters_.size() == 1 ?
                        selectedPluginFilters_.front() :
                        TranslateFormat("{} selected", selectedPluginFilters_.size());
                const auto pluginPickerLayout = ConstrainRecordPickerLayout(
                    ImGuiMCP::GetContentRegionAvail().x,
                    ImGuiMCP::GetFrameHeightWithSpacing());
                ImGuiMCP::SetNextItemWidth(pluginPickerLayout.controlWidth);
                ImGuiMCP::SetNextWindowSizeConstraints(
                    {pluginPickerLayout.controlWidth, 0.0F},
                    {pluginPickerLayout.popupMaxWidth, pluginPickerLayout.popupMaxHeight});
                if (ImGuiMCP::BeginCombo(
                        "##WhereaboutsPluginPicker", pluginPreview.c_str())) {
                    if (ImGuiMCP::Selectable(
                            TranslateText("Any"), selectedPluginFilters_.empty())) {
                        selectedPluginFilters_.clear();
                        searchOptionsChanged = true;
                    }
                    if (ImGuiMCP::IsWindowAppearing()) ImGuiMCP::SetKeyboardFocusHere();
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    static_cast<void>(ImGuiMCP::InputText(
                        "##WhereaboutsPluginPickerSearch",
                        pluginFilter_.data(),
                        pluginFilter_.size()));
                    const auto matches = FilterTextPickerOptions(
                        pluginOptions, pluginFilter_.data(), 50);
                    for (const auto optionIndex : matches.indices) {
                        const auto& option = pluginOptions[optionIndex];
                        const auto selected = std::ranges::any_of(
                            selectedPluginFilters_,
                            [&](const auto& value) {
                                return SearchTextEqualsNoexcept(value, option);
                            });
                        const auto optionWidth = ImGuiMCP::GetContentRegionAvail().x;
                        if (ImGuiMCP::Selectable(
                                option.c_str(),
                                selected,
                                ImGuiMCP::ImGuiSelectableFlags_DontClosePopups)) {
                            if (selected) {
                                std::erase_if(selectedPluginFilters_, [&](const auto& value) {
                                    return SearchTextEqualsNoexcept(value, option);
                                });
                            } else {
                                selectedPluginFilters_.push_back(option);
                            }
                            searchOptionsChanged = true;
                        }
                        OverflowTooltip(option, optionWidth);
                    }
                    if (matches.total == 0) {
                        ImGuiMCP::TextDisabled("%s", TranslateText("No matching records."));
                    } else if (matches.total > matches.indices.size()) {
                        const auto prompt = TranslateFormat(
                            "{} matches; type to narrow.", matches.total);
                        ImGuiMCP::TextDisabled("%s", prompt.c_str());
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Select one or more plugins."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                drawControlLabel(TranslateText("Location"));
                std::vector<std::string> locationOptions;
                if (pickerView && pickerView->catalog && pickerView->locations) {
                    locationOptions = ObservedLocationPickerOptions(
                        *pickerView->catalog, *pickerView->locations);
                }
                const auto locationPreview = !exactLocationFilter_.empty() ?
                    exactLocationFilter_ : locationFilter_.front() != '\0' ?
                        std::string{locationFilter_.data()} : TranslateOwned("Any");
                const auto locationPickerLayout = ConstrainRecordPickerLayout(
                    ImGuiMCP::GetContentRegionAvail().x,
                    ImGuiMCP::GetFrameHeightWithSpacing());
                ImGuiMCP::SetNextItemWidth(locationPickerLayout.controlWidth);
                ImGuiMCP::SetNextWindowSizeConstraints(
                    {locationPickerLayout.controlWidth, 0.0F},
                    {locationPickerLayout.popupMaxWidth, locationPickerLayout.popupMaxHeight});
                if (ImGuiMCP::BeginCombo(
                        "##WhereaboutsLocationPicker", locationPreview.c_str())) {
                    if (ImGuiMCP::Selectable(
                            TranslateText("Any"),
                            exactLocationFilter_.empty() && locationFilter_.front() == '\0')) {
                        exactLocationFilter_.clear();
                        locationFilter_.fill('\0');
                        searchOptionsChanged = true;
                    }
                    if (ImGuiMCP::IsWindowAppearing()) ImGuiMCP::SetKeyboardFocusHere();
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    if (ImGuiMCP::InputText(
                            "##WhereaboutsLocationPickerSearch",
                            locationFilter_.data(),
                            locationFilter_.size())) {
                        exactLocationFilter_.clear();
                        searchOptionsChanged = true;
                    }
                    const auto matches = FilterTextPickerOptions(
                        locationOptions, locationFilter_.data(), 50);
                    for (const auto optionIndex : matches.indices) {
                        const auto& option = locationOptions[optionIndex];
                        const auto optionWidth = ImGuiMCP::GetContentRegionAvail().x;
                        if (ImGuiMCP::Selectable(
                                option.c_str(),
                                SearchTextEqualsNoexcept(exactLocationFilter_, option))) {
                            exactLocationFilter_ = option;
                            locationFilter_.fill('\0');
                            const auto copyCount = (std::min)(
                                option.size(), locationFilter_.size() - 1);
                            std::copy_n(option.data(), copyCount, locationFilter_.data());
                            searchOptionsChanged = true;
                        }
                        OverflowTooltip(option, optionWidth);
                    }
                    if (matches.total == 0) {
                        ImGuiMCP::TextDisabled("%s", TranslateText("No matching records."));
                    } else if (matches.total > matches.indices.size()) {
                        const auto prompt = TranslateFormat(
                            "{} matches; type to narrow.", matches.total);
                        ImGuiMCP::TextDisabled("%s", prompt.c_str());
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText(
                    "Type for contains matching, or select an observed location for an exact match."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                drawControlLabel(TranslateText("Search content"));
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo(
                        "##WhereaboutsSearchContent",
                        contentNames[std::clamp(contentIndex, 0, 2)])) {
                    for (int index = 0; index < static_cast<int>(contentNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(contentNames[index], contentIndex == index)) {
                            searchContent_ = static_cast<SearchContent>(index);
                            searchOptionsChanged = true;
                            catalogModeChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                if (searchContent_ == SearchContent::NpcsAndLocations) {
                    static_cast<void>(ImGuiMCP::TableNextColumn());
                    const std::array orderNames{
                        TranslateText("NPCs first"), TranslateText("Locations first")};
                    int orderIndex = static_cast<int>(resultSectionOrder_);
                    drawControlLabel(TranslateText("Result order"));
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    if (ImGuiMCP::BeginCombo(
                            "##WhereaboutsResultOrder",
                            orderNames[std::clamp(orderIndex, 0, 1)])) {
                        for (int index = 0; index < static_cast<int>(orderNames.size()); ++index) {
                            if (ImGuiMCP::Selectable(orderNames[index], orderIndex == index)) {
                                resultSectionOrder_ = static_cast<ResultSectionOrder>(index);
                                searchOptionsChanged = true;
                                catalogModeChanged = true;
                            }
                        }
                        ImGuiMCP::EndCombo();
                    }
                }
                ImGuiMCP::EndTable();
            }

            ImGuiMCP::Spacing();
            const std::array stateNames{TranslateText("Any"), TranslateText("Yes"), TranslateText("No")};
            const auto stateFilter = [&](const char* id, const char* label, int& value, const char* tooltip) {
                bool changed = false;
                ImGuiMCP::SetNextItemWidth(-1.0F);
                const auto stateIndex = std::clamp(value, 0, 2);
                const auto preview = TriStateFilterPreview(label, stateNames[stateIndex]);
                if (ImGuiMCP::BeginCombo(id, preview.c_str())) {
                    for (int index = 0; index < static_cast<int>(stateNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(stateNames[index], value == index)) {
                            value = index;
                            changed = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(tooltip);
                return changed;
            };
            ImGuiMCP::BeginDisabled(!IncludesNpcs(searchContent_));
            const std::array stateLabels{
                TranslateText("Alive"), TranslateText("Enabled"),
                TranslateText(kFollowerFilterLabel), TranslateText("Potential Follower"),
                TranslateText("Loaded")};
            float stateControlWidth = 180.0F;
            for (const auto* label : stateLabels) {
                for (const auto* state : stateNames) {
                    const auto preview = TriStateFilterPreview(label, state);
                    stateControlWidth = (std::max)(
                        stateControlWidth,
                        ImGuiMCP::CalcTextSize(preview.c_str()).x +
                            ImGuiMCP::GetFrameHeightWithSpacing());
                }
            }
            const int stateColumns = ResponsiveControlColumns(
                filterWidth, stateControlWidth, filterSpacing, 5);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsStateFilters",
                    stateColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= stateFilter(
                    "##WhereaboutsAliveFilter", TranslateText("Alive"), aliveFilter_,
                    TranslateText("Any ignores this status. Yes requires it. No excludes it."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= stateFilter(
                    "##WhereaboutsEnabledFilter", TranslateText("Enabled"), enabledFilter_,
                    TranslateText("Any ignores this status. Yes requires it. No excludes it."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= stateFilter(
                    "##WhereaboutsFollowerFilter", TranslateText(kFollowerFilterLabel), teammateFilter_,
                    TranslateText("Currently following. Any ignores it; Yes requires it; No excludes it."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= stateFilter(
                    "##WhereaboutsPotentialFollowerFilter", TranslateText("Potential Follower"), potentialFollowerFilter_,
                    TranslateText("Potential-follower faction. Any ignores it; Yes requires it; No excludes it."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= stateFilter(
                    "##WhereaboutsLoadedFilter", TranslateText("Loaded"), loadedFilter_,
                    TranslateText("Any ignores this status. Yes requires it. No excludes it."));
                ImGuiMCP::EndTable();
            }

        }
        const auto renderAdvancedFilters = [&] {

            const auto advancedFilters = BuildSearchFilters();
            const auto advancedCount = ActiveAdvancedSearchFilterCount(advancedFilters);
            std::vector<FilterHeadingEntry> advancedHeadingEntries;
            advancedHeadingEntries.reserve(9);
            const auto addAdvancedSummary = [&](const char* label, const char* value) {
                advancedHeadingEntries.push_back({
                    label,
                    TriStateFilterPreview(label, value)});
            };
            if (advancedFilters.race || advancedFilters.unknownRaceOnly) {
                addAdvancedSummary(
                    TranslateText("Race"),
                    advancedFilters.unknownRaceOnly ? TranslateText("Unknown") : raceFilter_.c_str());
            }
            if (advancedFilters.sex) {
                const std::array values{
                    TranslateText("Any"), TranslateText("Male"),
                    TranslateText("Female"), TranslateText("Unknown")};
                addAdvancedSummary(
                    TranslateText("Sex"), values[std::clamp(sexFilter_, 0, 3)]);
            }
            const auto addKnownFlagSummary = [&](const char* label, KnownBooleanFilter value) {
                const char* state = TranslateText("Any");
                if (value == KnownBooleanFilter::Yes) state = TranslateText("Yes");
                else if (value == KnownBooleanFilter::No) state = TranslateText("No");
                else if (value == KnownBooleanFilter::Unknown) state = TranslateText("Unknown");
                addAdvancedSummary(label, state);
            };
            if (advancedFilters.essential != KnownBooleanFilter::Any) {
                addKnownFlagSummary(TranslateText("Essential"), advancedFilters.essential);
            }
            if (advancedFilters.protectedActor != KnownBooleanFilter::Any) {
                addKnownFlagSummary(TranslateText("Protected"), advancedFilters.protectedActor);
            }
            if (advancedFilters.spatialKind) {
                const std::array values{
                    TranslateText("Interior"), TranslateText("Exterior"), TranslateText("Unknown")};
                const auto index = *advancedFilters.spatialKind == SpatialKind::Interior ? 0 :
                    *advancedFilters.spatialKind == SpatialKind::Exterior ? 1 : 2;
                addAdvancedSummary(TranslateText("Area"), values[index]);
            }
            if (advancedFilters.spatialFreshness) {
                const std::array values{
                    TranslateText("Current"), TranslateText("Last observed"), TranslateText("Unavailable")};
                const auto index = *advancedFilters.spatialFreshness == SpatialFreshness::Current ? 0 :
                    *advancedFilters.spatialFreshness == SpatialFreshness::LastObserved ? 1 : 2;
                addAdvancedSummary(TranslateText("Location data"), values[index]);
            }
            if (advancedFilters.worldspaceFormID || advancedFilters.unknownWorldspaceOnly) {
                addAdvancedSummary(
                    TranslateText("Worldspace"),
                    advancedFilters.unknownWorldspaceOnly ? TranslateText("Unknown") :
                        worldspaceFilterLabel_.c_str());
            }
            if (advancedFilters.faction || advancedFilters.unknownFactionsOnly) {
                addAdvancedSummary(
                    TranslateText("Faction"),
                    advancedFilters.unknownFactionsOnly ? TranslateText("Unknown") :
                        factionFilterLabel_.c_str());
            }
            if (advancedFilters.baseKeyword || advancedFilters.unknownBaseKeywordsOnly) {
                addAdvancedSummary(
                    TranslateText("Base keyword"),
                    advancedFilters.unknownBaseKeywordsOnly ? TranslateText("Unknown") :
                        baseKeywordFilterLabel_.c_str());
            }
            const auto advancedBaseHeading = TranslateOwned("Advanced filters");
            const auto advancedCountHeading = advancedCount == 0 ?
                advancedBaseHeading :
                TranslateFormat("Advanced filters ({} active)", advancedCount);
            const auto advancedHeading = BuildFilterHeading(
                advancedCountHeading,
                advancedHeadingEntries,
                settings_.showActiveFilterNames,
                filterWidth,
                [&](std::string_view names) {
                    return TranslateFormat(
                        "{} ({} active: {})",
                        advancedBaseHeading,
                        advancedCount,
                        names);
                },
                [](std::string_view text) {
                    return ImGuiMCP::CalcTextSize(text.data(), text.data() + text.size()).x;
                });
            const auto advancedHeadingId = std::format(
                "{}###WhereaboutsAdvancedFilters", advancedHeading.heading);
            const bool advancedOpen = ImGuiMCP::CollapsingHeader(advancedHeadingId.c_str());
            if (ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_DelayNormal)) {
                const auto& tooltip = advancedHeading.tooltip.empty() ?
                    TranslateOwned("No advanced filters active.") :
                    advancedHeading.tooltip;
                ImGuiMCP::SetTooltip("%s", tooltip.c_str());
            }
            if (advancedOpen) {

            const std::array sexNames{
                TranslateText("Any"), TranslateText("Male"),
                TranslateText("Female"), TranslateText("Unknown")};
            const std::array knownStateNames{
                TranslateText("Any"), TranslateText("Yes"),
                TranslateText("No"), TranslateText("Unknown")};
            const auto racePreviewValue = unknownRaceOnly_ ? TranslateText("Unknown") :
                (raceFilter_.empty() ? TranslateText("Any") : raceFilter_.c_str());
            const auto racePreview = TriStateFilterPreview(
                TranslateText("Race"), racePreviewValue);
            const auto sexPreview = TriStateFilterPreview(
                TranslateText("Sex"), sexNames[std::clamp(sexFilter_, 0, 3)]);
            const auto essentialPreview = TriStateFilterPreview(
                TranslateText("Essential"),
                knownStateNames[std::clamp(essentialFilter_, 0, 3)]);
            const auto protectedPreview = TriStateFilterPreview(
                TranslateText("Protected"),
                knownStateNames[std::clamp(protectedFilter_, 0, 3)]);
            const std::array demographicPreviews{
                racePreview.c_str(), sexPreview.c_str(),
                essentialPreview.c_str(), protectedPreview.c_str()};
            const int demographicColumns = ResponsiveControlColumns(
                filterWidth,
                measuredControlWidth(demographicPreviews),
                filterSpacing,
                4);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsDemographicFilters",
                    demographicColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                static_cast<void>(ImGuiMCP::TableNextColumn());
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsRaceFilter", racePreview.c_str())) {
                    if (ImGuiMCP::Selectable(
                            TranslateText("Any"), raceFilter_.empty() && !unknownRaceOnly_)) {
                        raceFilter_.clear();
                        unknownRaceOnly_ = false;
                        searchOptionsChanged = true;
                    }
                    if (ImGuiMCP::Selectable(TranslateText("Unknown"), unknownRaceOnly_)) {
                        raceFilter_.clear();
                        unknownRaceOnly_ = true;
                        searchOptionsChanged = true;
                    }
                    if (const auto snapshots = index_.Snapshot(); snapshots && snapshots->catalog) {
                        for (const auto& race : DemographicRaceOptions(*snapshots->catalog)) {
                            if (ImGuiMCP::Selectable(
                                    race.c_str(), !unknownRaceOnly_ &&
                                        SearchTextEqualsNoexcept(raceFilter_, race))) {
                                raceFilter_ = race;
                                unknownRaceOnly_ = false;
                                searchOptionsChanged = true;
                            }
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Select an observed NPC race."));

                static_cast<void>(ImGuiMCP::TableNextColumn());
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsSexFilter", sexPreview.c_str())) {
                    for (int index = 0; index < static_cast<int>(sexNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(sexNames[index], sexFilter_ == index)) {
                            sexFilter_ = index;
                            searchOptionsChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }

                const auto knownStateFilter = [&](const char* id, const std::string& preview, int& value) {
                    bool changed = false;
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    if (ImGuiMCP::BeginCombo(id, preview.c_str())) {
                        for (int index = 0; index < static_cast<int>(knownStateNames.size()); ++index) {
                            if (ImGuiMCP::Selectable(knownStateNames[index], value == index)) {
                                value = index;
                                changed = true;
                            }
                        }
                        ImGuiMCP::EndCombo();
                    }
                    DelayedTooltip(TranslateText("No means the flag is known to be absent."));
                    return changed;
                };
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= knownStateFilter(
                    "##WhereaboutsEssentialFilter", essentialPreview, essentialFilter_);
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= knownStateFilter(
                    "##WhereaboutsProtectedFilter", protectedPreview, protectedFilter_);
                ImGuiMCP::EndTable();
            }

            const std::array areaNames{
                TranslateText("Any"), TranslateText("Interior"),
                TranslateText("Exterior"), TranslateText("Unknown")};
            const std::array freshnessNames{
                TranslateText("Any"), TranslateText("Current"),
                TranslateText("Last observed"), TranslateText("Unavailable")};
            const auto areaPreview = TriStateFilterPreview(
                TranslateText("Area"), areaNames[std::clamp(spatialKindFilter_, 0, 3)]);
            const auto freshnessPreview = TriStateFilterPreview(
                TranslateText("Location data"),
                freshnessNames[std::clamp(spatialFreshnessFilter_, 0, 3)]);
            const auto* worldspaceValue = unknownWorldspaceOnly_ ? TranslateText("Unknown") :
                (worldspaceFilterFormID_ == 0 ? TranslateText("Any") :
                    worldspaceFilterLabel_.c_str());
            const auto worldspacePreview = TriStateFilterPreview(
                TranslateText("Worldspace"), worldspaceValue);
            const std::array spatialPreviews{
                areaPreview.c_str(), freshnessPreview.c_str(), worldspacePreview.c_str()};
            const int spatialColumns = ResponsiveControlColumns(
                filterWidth,
                measuredControlWidth(spatialPreviews),
                filterSpacing,
                3);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsSpatialFilters",
                    spatialColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                static_cast<void>(ImGuiMCP::TableNextColumn());
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsAreaFilter", areaPreview.c_str())) {
                    for (int index = 0; index < static_cast<int>(areaNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(areaNames[index], spatialKindFilter_ == index)) {
                            spatialKindFilter_ = index;
                            searchOptionsChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }

                static_cast<void>(ImGuiMCP::TableNextColumn());
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo(
                        "##WhereaboutsSpatialFreshnessFilter", freshnessPreview.c_str())) {
                    for (int index = 0; index < static_cast<int>(freshnessNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(
                                freshnessNames[index], spatialFreshnessFilter_ == index)) {
                            spatialFreshnessFilter_ = index;
                            searchOptionsChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Current is loaded; Last observed is unloaded."));

                static_cast<void>(ImGuiMCP::TableNextColumn());
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo(
                        "##WhereaboutsWorldspaceFilter", worldspacePreview.c_str())) {
                    if (ImGuiMCP::Selectable(
                            TranslateText("Any"),
                            worldspaceFilterFormID_ == 0 && !unknownWorldspaceOnly_)) {
                        worldspaceFilterFormID_ = 0;
                        unknownWorldspaceOnly_ = false;
                        worldspaceFilterLabel_.clear();
                        searchOptionsChanged = true;
                    }
                    if (ImGuiMCP::Selectable(TranslateText("Unknown"), unknownWorldspaceOnly_)) {
                        worldspaceFilterFormID_ = 0;
                        unknownWorldspaceOnly_ = true;
                        worldspaceFilterLabel_.clear();
                        searchOptionsChanged = true;
                    }
                    if (const auto snapshots = index_.Snapshot(); snapshots && snapshots->catalog) {
                        for (const auto& option : SpatialWorldspaceOptions(*snapshots->catalog)) {
                            if (ImGuiMCP::Selectable(
                                    option.label.c_str(), !unknownWorldspaceOnly_ &&
                                        worldspaceFilterFormID_ == option.formID)) {
                                worldspaceFilterFormID_ = option.formID;
                                unknownWorldspaceOnly_ = false;
                                worldspaceFilterLabel_ = option.label;
                                searchOptionsChanged = true;
                            }
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Interior cells normally have no worldspace."));
                ImGuiMCP::EndTable();
            }

            const auto recordView = index_.Snapshot();
            if (recordView && recordView->catalog &&
                (recordOptionsSession_ != recordView->session ||
                 recordOptionsRevision_ != recordView->revision)) {
                factionFilterOptions_ = RecordFilterOptions(
                    *recordView->catalog, RecordFacetCategory::Faction);
                keywordFilterOptions_ = RecordFilterOptions(
                    *recordView->catalog, RecordFacetCategory::Keyword);
                recordOptionsSession_ = recordView->session;
                recordOptionsRevision_ = recordView->revision;
                factionOptionResultsDirty_ = true;
                keywordOptionResultsDirty_ = true;
            }

            const auto recordFilter = [&]<std::size_t SearchSize>(
                const char* comboId,
                const char* searchId,
                const char* translatedLabel,
                std::optional<RecordFilterSelection>& selected,
                bool& unknownOnly,
                std::string& selectedLabel,
                std::array<char, SearchSize>& optionSearch,
                const std::vector<RecordFilterOption>& options,
                RecordOptionMatches& visible,
                bool& resultsDirty) {
                bool changed = false;
                const auto* value = unknownOnly ? TranslateText("Unknown") :
                    (selected ? selectedLabel.c_str() : TranslateText("Any"));
                const auto preview = TriStateFilterPreview(translatedLabel, value);
                const auto pickerLayout = ConstrainRecordPickerLayout(
                    ImGuiMCP::GetContentRegionAvail().x,
                    ImGuiMCP::GetFrameHeightWithSpacing());
                ImGuiMCP::SetNextItemWidth(pickerLayout.controlWidth);
                ImGuiMCP::SetNextWindowSizeConstraints(
                    {pickerLayout.controlWidth, 0.0F},
                    {pickerLayout.popupMaxWidth, pickerLayout.popupMaxHeight});
                if (ImGuiMCP::BeginCombo(comboId, preview.c_str())) {
                    if (ImGuiMCP::Selectable(
                            TranslateText("Any"), !selected && !unknownOnly)) {
                        selected.reset();
                        unknownOnly = false;
                        selectedLabel.clear();
                        changed = true;
                    }
                    if (ImGuiMCP::Selectable(TranslateText("Unknown"), unknownOnly)) {
                        selected.reset();
                        unknownOnly = true;
                        selectedLabel.clear();
                        changed = true;
                    }
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    if (ImGuiMCP::InputText(
                            searchId, optionSearch.data(), optionSearch.size())) {
                        resultsDirty = true;
                    }
                    if (resultsDirty) {
                        visible = FilterRecordOptions(options, optionSearch.data(), 50);
                        resultsDirty = false;
                    }
                    for (const auto index : visible.indices) {
                        const auto& option = options[index];
                        const auto availableWidth = ImGuiMCP::GetContentRegionAvail().x;
                        if (ImGuiMCP::Selectable(
                                option.label.c_str(), selected &&
                                    selected->runtimeFormID ==
                                        option.selection.runtimeFormID)) {
                            selected = option.selection;
                            unknownOnly = false;
                            selectedLabel = option.label;
                            changed = true;
                        }
                        OverflowTooltip(option.label, availableWidth);
                    }
                    if (visible.total == 0) {
                        ImGuiMCP::TextDisabled("%s", TranslateText("No matching records."));
                    } else if (visible.total > visible.indices.size()) {
                        const auto prompt = TranslateFormat(
                            "{} matches; type to narrow.", visible.total);
                        ImGuiMCP::TextDisabled("%s", prompt.c_str());
                    }
                    ImGuiMCP::EndCombo();
                }
                return changed;
            };

            const std::array recordPreviews{
                TriStateFilterPreview(
                    TranslateText("Faction"),
                    unknownFactionsOnly_ ? TranslateText("Unknown") :
                        (factionFilter_ ? factionFilterLabel_.c_str() : TranslateText("Any"))),
                TriStateFilterPreview(
                    TranslateText("Base keyword"),
                    unknownBaseKeywordsOnly_ ? TranslateText("Unknown") :
                        (baseKeywordFilter_ ? baseKeywordFilterLabel_.c_str() : TranslateText("Any")))};
            const std::array recordPreviewText{
                recordPreviews[0].c_str(), recordPreviews[1].c_str()};
            const int recordColumns = ResponsiveControlColumns(
                filterWidth,
                measuredControlWidth(recordPreviewText),
                filterSpacing,
                2);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsRecordFilters",
                    recordColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                const auto factionSearchLabel = std::format(
                    "{}##WhereaboutsFactionOptionSearch", TranslateText("Find faction"));
                const auto keywordSearchLabel = std::format(
                    "{}##WhereaboutsKeywordOptionSearch", TranslateText("Find keyword"));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= recordFilter(
                    "##WhereaboutsFactionFilter",
                    factionSearchLabel.c_str(),
                    TranslateText("Faction"),
                    factionFilter_,
                    unknownFactionsOnly_,
                    factionFilterLabel_,
                    factionOptionSearch_,
                    factionFilterOptions_,
                    visibleFactionOptions_,
                    factionOptionResultsDirty_);
                DelayedTooltip(TranslateText("Filter by an NPC's base faction."));

                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= recordFilter(
                    "##WhereaboutsBaseKeywordFilter",
                    keywordSearchLabel.c_str(),
                    TranslateText("Base keyword"),
                    baseKeywordFilter_,
                    unknownBaseKeywordsOnly_,
                    baseKeywordFilterLabel_,
                    keywordOptionSearch_,
                    keywordFilterOptions_,
                    visibleKeywordOptions_,
                    keywordOptionResultsDirty_);
                DelayedTooltip(TranslateText("Filter by a keyword on the base NPC."));
                ImGuiMCP::EndTable();
            }
            }
        };

        if (filtersOpen) {

            const std::array contextLabels{
                TranslateText("Favorites only"), TranslateText("Tracked only"),
                TranslateText("Same location"), TranslateText("Include Generic NPCs")};
            const int contextColumns = ResponsiveControlColumns(
                filterWidth,
                measuredControlWidth(contextLabels),
                filterSpacing,
                4);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsContextFilters",
                    contextColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= ImGuiMCP::Checkbox(
                    TranslateText("Favorites only"), &favoritesOnly_);
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= ImGuiMCP::Checkbox(
                    TranslateText("Tracked only"), &trackedOnly_);
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= ImGuiMCP::Checkbox(
                    TranslateText("Same location"), &sameLocationOnly_);
                DelayedTooltip(TranslateText("Same cell or assigned Location."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                searchOptionsChanged |= ImGuiMCP::Checkbox(
                    TranslateText("Include Generic NPCs"), &includeGeneric_);
                DelayedTooltip(TranslateText(
                    "Shows non-unique actors. Commands use the exact reference FormID."));
                ImGuiMCP::EndTable();
            }

            const bool newGenericAutoContext = sameLocationOnly_ || teammateFilter_ == 1;
            const auto genericState = TransitionGenericFilter(
                {includeGeneric_, genericAutoContext_}, newGenericAutoContext);
            if (genericState.includeGeneric != includeGeneric_ ||
                genericState.automaticContext != genericAutoContext_) {
                includeGeneric_ = genericState.includeGeneric;
                genericAutoContext_ = genericState.automaticContext;
                searchOptionsChanged = true;
            }
            if (!includeGeneric_) {
                genericOnly_ = false;
            }
            ImGuiMCP::EndDisabled();

            ImGuiMCP::Spacing();
            const std::array sortNames{
                TranslateText("Name"), TranslateText("Plugin"), TranslateText("Level"),
                TranslateText("NPC location"), TranslateText("Loaded"), TranslateText("Distance"),
                TranslateText("Status"), TranslateText("Random")};
            const std::array directionNames{
                TranslateText("Descending"), TranslateText("Ascending")};
            const std::array sortControlText{
                TranslateText("Clear Filters"),
                sortNames[0], sortNames[1], sortNames[2], sortNames[3],
                sortNames[4], sortNames[5], sortNames[6], sortNames[7],
                directionNames[0], directionNames[1], TranslateText("Not used")};
            const int sortControlColumns = ResponsiveControlColumns(
                filterWidth,
                measuredControlWidth(sortControlText),
                filterSpacing,
                3);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsSortAndActions",
                    sortControlColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                ImGuiMCP::BeginDisabled(!IncludesNpcs(searchContent_));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo(
                        "##WhereaboutsSortKey",
                        sortNames[std::clamp(sortIndex_, 0, 7)])) {
                    for (int index = 0; index < static_cast<int>(sortNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(sortNames[index], sortIndex_ == index)) {
                            sortIndex_ = index;
                            if (sortIndex_ == static_cast<int>(SortKey::Random)) {
                                randomSeed_ = NextRandomSeed(randomSeed_);
                            }
                            searchSortUiDirty_ = true;
                            searchOptionsChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Choose how NPC results are ordered."));
                static_cast<void>(ImGuiMCP::TableNextColumn());
                const auto selectedSort = static_cast<SortKey>(std::clamp(sortIndex_, 0, 7));
                const bool usesDirection = SortUsesDirection(selectedSort);
                ImGuiMCP::BeginDisabled(!SortUsesDirection(selectedSort));
                ImGuiMCP::SetNextItemWidth(-1.0F);
                const auto* directionPreview = usesDirection ?
                    directionNames[ascending_ ? 1 : 0] : TranslateText("Not used");
                if (ImGuiMCP::BeginCombo(
                        "##WhereaboutsSortDirection", directionPreview)) {
                    for (int index = 0; index < static_cast<int>(directionNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(directionNames[index], ascending_ == (index == 1))) {
                            ascending_ = index == 1;
                            searchSortUiDirty_ = true;
                            searchOptionsChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                ImGuiMCP::EndDisabled();
                DelayedTooltip(usesDirection ?
                    TranslateText("Choose ascending or descending order.") :
                    TranslateText("Random order does not use a direction."));
                ImGuiMCP::EndDisabled();
                static_cast<void>(ImGuiMCP::TableNextColumn());
                if (ImGuiMCP::Button(
                        TranslateText("Clear Filters"), {-1.0F, 0.0F})) {
                    ResetFiltersToDefaults();
                    searchOptionsChanged = true;
                    catalogModeChanged = true;
                }
                ImGuiMCP::EndTable();
            }
        }

        if (settings_.enableAdvancedFilters) renderAdvancedFilters();

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
                const bool superCompact = IsSuperCompactResultDensity(
                    settings_.resultDensity);
                if (IncludesNpcs(searchContent_) && ImGuiMCP::BeginTable(
                        superCompact ? "##WhereaboutsSuperCompactSearchResultTable" :
                                       "##WhereaboutsSearchResultTable",
                        superCompact ? 2 : 3,
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
                    const bool showSecondary = ShowsSecondaryResultMetadata(settings_.resultDensity);
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
                            ApplyUnifiedRowBackground(interaction, selected);
                            if (interaction.activated) {
                                SelectSnapshot(npc, TargetSource::Search);
                            }
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
                        ApplyUnifiedRowBackground(interaction, selected);
                        if (interaction.activated) SelectSnapshot(npc, TargetSource::Search);
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
            RenderDetails();
        }
        ImGuiMCP::EndChild();
        RenderRootModals();
    }
}
