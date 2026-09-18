#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "Search/RuntimeIndex.h"
#include "UI/FilterLayoutModel.h"
#include "UI/Menu.h"
#include "UI/MenuModel.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>

namespace whereabouts::ui
{
    void Menu::RenderSearchFilters(bool& searchOptionsChanged, bool& catalogModeChanged)
    {
        const float filterWidth = ImGuiMCP::GetContentRegionAvail().x;
        const auto* style = ImGuiMCP::GetStyle();
        const float filterSpacing = style ? style->ItemSpacing.x : 8.0F;
        const auto filters = BuildSearchFilters();
        const auto commonDensity = settings_.DensityFor(UiDensityArea::Filters);
        const auto advancedDensity = settings_.DensityFor(UiDensityArea::AdvancedFilters);
        const auto densityLabel = [&](
            ResultDensity density,
            const char* detailed,
            const char* compact,
            const char* superCompact) {
            const char* key = density == ResultDensity::Detailed ? detailed :
                density == ResultDensity::Compact ? compact : superCompact;
            return TranslateText(key);
        };
        const auto pushDensityStyle = [&](ResultDensity density) {
            if (density == ResultDensity::Detailed || !style) return 0;
            const float scale = density == ResultDensity::Compact ? 0.78F : 0.58F;
            ImGuiMCP::PushStyleVar(
                ImGuiMCP::ImGuiStyleVar_FramePadding,
                {style->FramePadding.x, (std::max)(1.0F, style->FramePadding.y * scale)});
            ImGuiMCP::PushStyleVar(
                ImGuiMCP::ImGuiStyleVar_ItemSpacing,
                {style->ItemSpacing.x, (std::max)(1.0F, style->ItemSpacing.y * scale)});
            return 2;
        };
        const auto renderRows = [&](
            std::string_view idPrefix,
            const FilterLayoutPlan& plan,
            auto&& renderControl) {
            for (std::size_t rowIndex = 0; rowIndex < plan.rows.size(); ++rowIndex) {
                const auto& row = plan.rows[rowIndex];
                const auto tableId = std::format("##{}Row{}", idPrefix, rowIndex);
                if (!ImGuiMCP::BeginTable(
                        tableId.c_str(), static_cast<int>(row.cells.size()),
                        ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
                    continue;
                }
                for (std::size_t cellIndex = 0; cellIndex < row.cells.size(); ++cellIndex) {
                    const auto columnId = std::format("##{}Column{}", idPrefix, cellIndex);
                    ImGuiMCP::TableSetupColumn(
                        columnId.c_str(), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch,
                        row.cells[cellIndex].weight);
                }
                for (const auto& cell : row.cells) {
                    static_cast<void>(ImGuiMCP::TableNextColumn());
                    renderControl(cell.control);
                }
                ImGuiMCP::EndTable();
            }
        };

        std::vector<FilterHeadingEntry> commonEntries;
        const auto addCommon = [&](const char* label, std::string value) {
            commonEntries.push_back({label, TriStateFilterPreview(label, value.c_str())});
        };
        if (!filters.selectedPlugins.empty()) {
            std::string plugins;
            for (const auto& plugin : filters.selectedPlugins) {
                if (!plugins.empty()) plugins.append(", ");
                plugins.append(plugin);
            }
            addCommon(TranslateText("Plugin"), std::move(plugins));
        } else if (filters.plugin) {
            addCommon(TranslateText("Plugin"), *filters.plugin);
        }
        if (filters.location) addCommon(TranslateText("Location"), *filters.location);
        const auto addKnownCommon = [&](const char* label, std::optional<bool> value) {
            if (value) addCommon(label, TranslateOwned(*value ? "Yes" : "No"));
        };
        addKnownCommon(TranslateText("Alive"), filters.alive);
        addKnownCommon(TranslateText("Enabled"), filters.enabled);
        addKnownCommon(TranslateText(kFollowerFilterLabel), filters.teammate);
        addKnownCommon(TranslateText("Potential Follower"), filters.potentialFollower);
        addKnownCommon(TranslateText("Loaded"), filters.loaded);
        if (filters.favoritesOnly) addCommon(TranslateText("Favorites only"), TranslateOwned("Yes"));
        if (filters.trackedOnly) addCommon(TranslateText("Tracked only"), TranslateOwned("Yes"));
        if (filters.sameLocationOnly) addCommon(TranslateText("Same location"), TranslateOwned("Yes"));
        if (filters.includeGeneric) addCommon(TranslateText("Include Generic NPCs"), TranslateOwned("Yes"));
        const auto commonCount = ActiveCommonSearchFilterCount(filters);
        const auto commonBase = TranslateOwned("Filters and sorting");
        const auto commonCountHeading = commonCount == 0 ? commonBase :
            TranslateFormat("Filters and sorting ({} active)", commonCount);
        const auto commonHeading = BuildFilterHeading(
            commonCountHeading, commonEntries, settings_.showActiveFilterNames, filterWidth,
            [&](std::string_view names) {
                return TranslateFormat("{} ({} active: {})", commonBase, commonCount, names);
            },
            [](std::string_view text) {
                return ImGuiMCP::CalcTextSize(text.data(), text.data() + text.size()).x;
            });
        const auto commonHeadingId = std::format(
            "{}###WhereaboutsFilters", commonHeading.heading);
        const bool commonOpen = ImGuiMCP::CollapsingHeader(commonHeadingId.c_str());
        if (!commonHeading.tooltip.empty() &&
            ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_DelayNormal)) {
            ImGuiMCP::SetTooltip("%s", commonHeading.tooltip.c_str());
        }

        if (commonOpen) {
            const int pushedStyle = pushDensityStyle(commonDensity);
            std::optional<std::size_t> removePlugin;
            if (commonDensity == ResultDensity::Detailed) {
                float remaining = filterWidth;
                for (std::size_t index = 0; index < selectedPluginFilters_.size(); ++index) {
                    const float chipWidth =
                        ImGuiMCP::CalcTextSize(selectedPluginFilters_[index].c_str()).x + 48.0F;
                    if (index > 0 && chipWidth <= remaining) ImGuiMCP::SameLine();
                    else remaining = filterWidth;
                    ImGuiMCP::PushID(static_cast<int>(index));
                    if (ImGuiMCP::SmallButton("X")) removePlugin = index;
                    DelayedTooltip(TranslateText("Remove this plugin filter."));
                    ImGuiMCP::SameLine();
                    const float textWidth = ImGuiMCP::GetContentRegionAvail().x;
                    ImGuiMCP::TextDisabled("%s", selectedPluginFilters_[index].c_str());
                    OverflowTooltip(selectedPluginFilters_[index], textWidth);
                    ImGuiMCP::PopID();
                    remaining -= chipWidth;
                }
            }
            if (removePlugin) {
                selectedPluginFilters_.erase(
                    selectedPluginFilters_.begin() + static_cast<std::ptrdiff_t>(*removePlugin));
                searchOptionsChanged = true;
            }

            const auto view = index_.Snapshot();
            std::vector<std::string> pluginNames;
            if (view && view->catalog) {
                pluginNames.reserve(view->catalog->size());
                for (const auto& npc : *view->catalog) {
                    if (!npc.SourcePlugin().empty()) pluginNames.emplace_back(npc.SourcePlugin());
                }
            }
            if (view && view->locations && IncludesLocations(searchContent_)) {
                for (const auto& location : *view->locations) {
                    if (!location.SourcePlugin().empty()) pluginNames.emplace_back(location.SourcePlugin());
                }
            }
            const auto pluginOptions = BuildTextPickerOptions(pluginNames);
            std::vector<std::string> locationOptions;
            if (view && view->catalog && view->locations) {
                locationOptions = ObservedLocationPickerOptions(*view->catalog, *view->locations);
            }
            const std::array contentNames{
                TranslateText("NPCs only"), TranslateText("NPCs and locations"),
                TranslateText("Locations only")};
            const std::array stateNames{
                TranslateText("Any"), TranslateText("Yes"), TranslateText("No")};
            const std::array sortNames{
                TranslateText("Name"), TranslateText("Plugin"), TranslateText("Level"),
                TranslateText("NPC location"), TranslateText("Loaded"), TranslateText("Distance"),
                TranslateText("Status"), TranslateText("Random")};
            const std::array directionNames{
                TranslateText("Descending"), TranslateText("Ascending")};
            const auto stateFilter = [&](const char* id, const char* label, int& value, const char* tooltip) {
                bool changed = false;
                const auto preview = TriStateFilterPreview(label, stateNames[std::clamp(value, 0, 2)]);
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo(id, preview.c_str())) {
                    for (int index = 0; index < 3; ++index) {
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
            const auto renderCommonControl = [&](FilterControl control) {
                const bool npcOnly = control != FilterControl::Plugin &&
                    control != FilterControl::Location && control != FilterControl::SearchContent &&
                    control != FilterControl::ResultSectionOrder &&
                    control != FilterControl::ClearCommon;
                if (npcOnly) ImGuiMCP::BeginDisabled(!IncludesNpcs(searchContent_));
                switch (control) {
                case FilterControl::Plugin: {
                    const auto value = selectedPluginFilters_.empty() ? TranslateOwned("Any") :
                        selectedPluginFilters_.size() == 1 ? selectedPluginFilters_.front() :
                        TranslateFormat("{} selected", selectedPluginFilters_.size());
                    const auto preview = TriStateFilterPreview(TranslateText("Plugin"), value.c_str());
                    const auto layout = ConstrainRecordPickerLayout(
                        ImGuiMCP::GetContentRegionAvail().x, ImGuiMCP::GetFrameHeightWithSpacing());
                    ImGuiMCP::SetNextItemWidth(layout.controlWidth);
                    ImGuiMCP::SetNextWindowSizeConstraints(
                        {layout.popupMinWidth, 0.0F}, {layout.popupMaxWidth, layout.popupMaxHeight});
                    if (ImGuiMCP::BeginCombo("##WhereaboutsPluginPicker", preview.c_str())) {
                        if (ImGuiMCP::Selectable(TranslateText("Any"), selectedPluginFilters_.empty())) {
                            selectedPluginFilters_.clear();
                            searchOptionsChanged = true;
                        }
                        if (ImGuiMCP::IsWindowAppearing()) ImGuiMCP::SetKeyboardFocusHere();
                        ImGuiMCP::SetNextItemWidth(-1.0F);
                        static_cast<void>(ImGuiMCP::InputText(
                            "##WhereaboutsPluginPickerSearch", pluginFilter_.data(), pluginFilter_.size()));
                        const auto matches = FilterTextPickerOptions(pluginOptions, pluginFilter_.data(), 50);
                        for (const auto optionIndex : matches.indices) {
                            const auto& option = pluginOptions[optionIndex];
                            const bool selected = std::ranges::any_of(
                                selectedPluginFilters_, [&](const auto& current) {
                                    return SearchTextEqualsNoexcept(current, option);
                                });
                            const float width = ImGuiMCP::GetContentRegionAvail().x;
                            if (ImGuiMCP::Selectable(
                                    option.c_str(), selected,
                                    ImGuiMCP::ImGuiSelectableFlags_DontClosePopups)) {
                                if (selected) {
                                    std::erase_if(selectedPluginFilters_, [&](const auto& current) {
                                        return SearchTextEqualsNoexcept(current, option);
                                    });
                                } else selectedPluginFilters_.push_back(option);
                                searchOptionsChanged = true;
                            }
                            OverflowTooltip(option, width);
                        }
                        if (matches.total == 0) {
                            ImGuiMCP::TextDisabled("%s", TranslateText("No matching records."));
                        } else if (matches.total > matches.indices.size()) {
                            const auto text = TranslateFormat("{} matches; type to narrow.", matches.total);
                            ImGuiMCP::TextDisabled("%s", text.c_str());
                        }
                        ImGuiMCP::EndCombo();
                    }
                    DelayedTooltip(TranslateText("Select one or more plugins."));
                    break;
                }
                case FilterControl::Location: {
                    const auto value = !exactLocationFilter_.empty() ? exactLocationFilter_ :
                        locationFilter_.front() != '\0' ? std::string{locationFilter_.data()} :
                        TranslateOwned("Any");
                    const auto preview = TriStateFilterPreview(TranslateText("Location"), value.c_str());
                    const auto layout = ConstrainRecordPickerLayout(
                        ImGuiMCP::GetContentRegionAvail().x, ImGuiMCP::GetFrameHeightWithSpacing());
                    ImGuiMCP::SetNextItemWidth(layout.controlWidth);
                    ImGuiMCP::SetNextWindowSizeConstraints(
                        {layout.popupMinWidth, 0.0F}, {layout.popupMaxWidth, layout.popupMaxHeight});
                    if (ImGuiMCP::BeginCombo("##WhereaboutsLocationPicker", preview.c_str())) {
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
                                locationFilter_.data(), locationFilter_.size())) {
                            exactLocationFilter_.clear();
                            searchOptionsChanged = true;
                        }
                        const auto matches = FilterTextPickerOptions(
                            locationOptions, locationFilter_.data(), 50);
                        for (const auto optionIndex : matches.indices) {
                            const auto& option = locationOptions[optionIndex];
                            const float width = ImGuiMCP::GetContentRegionAvail().x;
                            if (ImGuiMCP::Selectable(
                                    option.c_str(), SearchTextEqualsNoexcept(exactLocationFilter_, option))) {
                                exactLocationFilter_ = option;
                                locationFilter_.fill('\0');
                                std::copy_n(option.data(),
                                    (std::min)(option.size(), locationFilter_.size() - 1),
                                    locationFilter_.data());
                                searchOptionsChanged = true;
                            }
                            OverflowTooltip(option, width);
                        }
                        if (matches.total == 0) {
                            ImGuiMCP::TextDisabled("%s", TranslateText("No matching records."));
                        } else if (matches.total > matches.indices.size()) {
                            const auto text = TranslateFormat("{} matches; type to narrow.", matches.total);
                            ImGuiMCP::TextDisabled("%s", text.c_str());
                        }
                        ImGuiMCP::EndCombo();
                    }
                    DelayedTooltip(TranslateText(
                        "Type for contains matching, or select an observed location for an exact match."));
                    break;
                }
                case FilterControl::SearchContent: {
                    int selected = static_cast<int>(searchContent_);
                    const auto label = densityLabel(
                        commonDensity, "Search content", "Content", "Content");
                    const auto preview = TriStateFilterPreview(label, contentNames[std::clamp(selected, 0, 2)]);
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    if (ImGuiMCP::BeginCombo("##WhereaboutsSearchContent", preview.c_str())) {
                        for (int index = 0; index < 3; ++index) {
                            if (ImGuiMCP::Selectable(contentNames[index], selected == index)) {
                                searchContent_ = static_cast<SearchContent>(index);
                                searchOptionsChanged = true;
                                catalogModeChanged = true;
                            }
                        }
                        ImGuiMCP::EndCombo();
                    }
                    DelayedTooltip(TranslateText("Choose whether Search returns NPCs, locations, or both."));
                    break;
                }
                case FilterControl::ResultSectionOrder: {
                    const std::array names{TranslateText("NPCs first"), TranslateText("Locations first")};
                    int selected = static_cast<int>(resultSectionOrder_);
                    const auto preview = TriStateFilterPreview(
                        densityLabel(commonDensity, "Result order", "Order", "Order"),
                        names[std::clamp(selected, 0, 1)]);
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    if (ImGuiMCP::BeginCombo("##WhereaboutsResultOrder", preview.c_str())) {
                        for (int index = 0; index < 2; ++index) {
                            if (ImGuiMCP::Selectable(names[index], selected == index)) {
                                resultSectionOrder_ = static_cast<ResultSectionOrder>(index);
                                searchOptionsChanged = true;
                                catalogModeChanged = true;
                            }
                        }
                        ImGuiMCP::EndCombo();
                    }
                    DelayedTooltip(TranslateText("Result order"));
                    break;
                }
                case FilterControl::Alive:
                    searchOptionsChanged |= stateFilter("##WhereaboutsAliveFilter", TranslateText("Alive"),
                        aliveFilter_, TranslateText("Any ignores this status. Yes requires it. No excludes it."));
                    break;
                case FilterControl::Enabled:
                    searchOptionsChanged |= stateFilter("##WhereaboutsEnabledFilter", TranslateText("Enabled"),
                        enabledFilter_, TranslateText("Any ignores this status. Yes requires it. No excludes it."));
                    break;
                case FilterControl::Loaded:
                    searchOptionsChanged |= stateFilter("##WhereaboutsLoadedFilter", TranslateText("Loaded"),
                        loadedFilter_, TranslateText("Any ignores this status. Yes requires it. No excludes it."));
                    break;
                case FilterControl::Follower:
                    searchOptionsChanged |= stateFilter("##WhereaboutsFollowerFilter", TranslateText(kFollowerFilterLabel),
                        teammateFilter_, TranslateText("Currently following. Any ignores it; Yes requires it; No excludes it."));
                    break;
                case FilterControl::PotentialFollower:
                    searchOptionsChanged |= stateFilter(
                        "##WhereaboutsPotentialFollowerFilter",
                        densityLabel(commonDensity, "Potential Follower", "Potential", "Potential"),
                        potentialFollowerFilter_, TranslateText(
                            "Potential-follower faction. Any ignores it; Yes requires it; No excludes it."));
                    break;
                case FilterControl::FavoritesOnly: {
                    const auto label = std::format("{}##WhereaboutsFavoritesOnly",
                        densityLabel(commonDensity, "Favorites only", "Favorites", "Fav"));
                    searchOptionsChanged |= ImGuiMCP::Checkbox(label.c_str(), &favoritesOnly_);
                    DelayedTooltip(TranslateText("Favorites only"));
                    break;
                }
                case FilterControl::TrackedOnly: {
                    const auto label = std::format("{}##WhereaboutsTrackedOnly",
                        densityLabel(commonDensity, "Tracked only", "Tracked", "Track"));
                    searchOptionsChanged |= ImGuiMCP::Checkbox(label.c_str(), &trackedOnly_);
                    DelayedTooltip(TranslateText("Tracked only"));
                    break;
                }
                case FilterControl::SameLocation: {
                    const auto label = std::format("{}##WhereaboutsSameLocation",
                        densityLabel(commonDensity, "Same location", "Same location", "Same"));
                    searchOptionsChanged |= ImGuiMCP::Checkbox(label.c_str(), &sameLocationOnly_);
                    DelayedTooltip(TranslateText("Same cell or assigned Location."));
                    break;
                }
                case FilterControl::IncludeGeneric: {
                    const auto label = std::format("{}##WhereaboutsIncludeGeneric",
                        densityLabel(commonDensity, "Include Generic NPCs", "Generic NPCs", "Generic"));
                    searchOptionsChanged |= ImGuiMCP::Checkbox(label.c_str(), &includeGeneric_);
                    DelayedTooltip(TranslateText(
                        "Shows non-unique actors. Commands use the exact reference FormID."));
                    break;
                }
                case FilterControl::Sort: {
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    if (ImGuiMCP::BeginCombo(
                            "##WhereaboutsSortKey", sortNames[std::clamp(sortIndex_, 0, 7)])) {
                        for (int index = 0; index < 8; ++index) {
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
                    break;
                }
                case FilterControl::Direction: {
                    const auto selectedSort = static_cast<SortKey>(std::clamp(sortIndex_, 0, 7));
                    const bool used = SortUsesDirection(selectedSort);
                    ImGuiMCP::BeginDisabled(!used);
                    ImGuiMCP::SetNextItemWidth(-1.0F);
                    const auto* preview = used ? directionNames[ascending_ ? 1 : 0] : TranslateText("Not used");
                    if (ImGuiMCP::BeginCombo("##WhereaboutsSortDirection", preview)) {
                        for (int index = 0; index < 2; ++index) {
                            if (ImGuiMCP::Selectable(directionNames[index], ascending_ == (index == 1))) {
                                ascending_ = index == 1;
                                searchSortUiDirty_ = true;
                                searchOptionsChanged = true;
                            }
                        }
                        ImGuiMCP::EndCombo();
                    }
                    ImGuiMCP::EndDisabled();
                    DelayedTooltip(used ? TranslateText("Choose ascending or descending order.") :
                        TranslateText("Random order does not use a direction."));
                    break;
                }
                case FilterControl::ClearCommon: {
                    const auto label = std::format(
                        "{}##WhereaboutsClearCommonFilters", TranslateText("Clear Filters"));
                    if (ImGuiMCP::Button(label.c_str(), {-1.0F, 0.0F})) {
                        ResetFiltersToDefaults();
                        searchOptionsChanged = true;
                        catalogModeChanged = true;
                    }
                    break;
                }
                default: break;
                }
                if (npcOnly) ImGuiMCP::EndDisabled();
            };
            renderRows(
                "WhereaboutsCommonFilters",
                BuildCommonFilterLayout(
                    commonDensity,
                    searchContent_ == SearchContent::NpcsAndLocations,
                    filterWidth, filterSpacing),
                renderCommonControl);

            const bool newAutomaticContext = sameLocationOnly_ || teammateFilter_ == 1;
            const auto generic = TransitionGenericFilter(
                {includeGeneric_, genericAutoContext_}, newAutomaticContext);
            if (generic.includeGeneric != includeGeneric_ ||
                generic.automaticContext != genericAutoContext_) {
                includeGeneric_ = generic.includeGeneric;
                genericAutoContext_ = generic.automaticContext;
                searchOptionsChanged = true;
            }
            if (!includeGeneric_) genericOnly_ = false;
            if (pushedStyle > 0) ImGuiMCP::PopStyleVar(pushedStyle);
        }

        if (!settings_.enableAdvancedFilters) return;

        std::vector<FilterHeadingEntry> advancedEntries;
        const auto addAdvanced = [&](const char* label, const char* value) {
            advancedEntries.push_back({label, TriStateFilterPreview(label, value)});
        };
        const auto addKnown = [&](const char* label, KnownBooleanFilter value) {
            const char* state = TranslateText("Any");
            if (value == KnownBooleanFilter::Yes) state = TranslateText("Yes");
            else if (value == KnownBooleanFilter::No) state = TranslateText("No");
            else if (value == KnownBooleanFilter::Unknown) state = TranslateText("Unknown");
            addAdvanced(label, state);
        };
        if (filters.race || filters.unknownRaceOnly) addAdvanced(
            TranslateText("Race"), filters.unknownRaceOnly ? TranslateText("Unknown") : raceFilter_.c_str());
        if (filters.sex) {
            const std::array values{TranslateText("Any"), TranslateText("Male"),
                TranslateText("Female"), TranslateText("Unknown")};
            addAdvanced(TranslateText("Sex"), values[std::clamp(sexFilter_, 0, 3)]);
        }
        if (filters.essential != KnownBooleanFilter::Any) addKnown(TranslateText("Essential"), filters.essential);
        if (filters.protectedActor != KnownBooleanFilter::Any) addKnown(TranslateText("Protected"), filters.protectedActor);
        if (filters.spatialKind) addAdvanced(TranslateText("Area"),
            *filters.spatialKind == SpatialKind::Interior ? TranslateText("Interior") :
            *filters.spatialKind == SpatialKind::Exterior ? TranslateText("Exterior") : TranslateText("Unknown"));
        if (filters.spatialFreshness) addAdvanced(TranslateText("Location data"),
            *filters.spatialFreshness == SpatialFreshness::Current ? TranslateText("Current") :
            *filters.spatialFreshness == SpatialFreshness::LastObserved ? TranslateText("Last observed") :
            TranslateText("Unavailable"));
        if (filters.worldspaceFormID || filters.unknownWorldspaceOnly) addAdvanced(
            TranslateText("Worldspace"), filters.unknownWorldspaceOnly ?
                TranslateText("Unknown") : worldspaceFilterLabel_.c_str());
        if (filters.faction || filters.unknownFactionsOnly) addAdvanced(
            TranslateText("Faction"), filters.unknownFactionsOnly ?
                TranslateText("Unknown") : factionFilterLabel_.c_str());
        if (filters.baseKeyword || filters.unknownBaseKeywordsOnly) addAdvanced(
            TranslateText("Base keyword"), filters.unknownBaseKeywordsOnly ?
                TranslateText("Unknown") : baseKeywordFilterLabel_.c_str());
        const auto addPlugins = [&](const char* label, const std::vector<std::string>& values) {
            if (values.empty()) return;
            const auto text = values.size() == 1 ? values.front() :
                TranslateFormat("{} selected", values.size());
            addAdvanced(label, text.c_str());
        };
        addPlugins(TranslateText("Touches NPC record"), filters.touchingPlugins);
        addPlugins(TranslateText("Original plugin"), filters.originalPlugins);
        addPlugins(TranslateText("Winning plugin"), filters.winningPlugins);
        if (filters.multiplePluginRecords != KnownBooleanFilter::Any) addKnown(
            TranslateText("Multiple plugin records"), filters.multiplePluginRecords);
        if (filters.minimumPluginRecordCount) {
            const auto value = std::to_string(*filters.minimumPluginRecordCount);
            addAdvanced(TranslateText("Min plugin records"), value.c_str());
        }
        if (filters.maximumPluginRecordCount) {
            const auto value = std::to_string(*filters.maximumPluginRecordCount);
            addAdvanced(TranslateText("Max plugin records"), value.c_str());
        }
        if (filters.npcClass || filters.unknownClassOnly) addAdvanced(
            TranslateText("Class"), filters.unknownClassOnly ? TranslateText("Unknown") : classFilterLabel_.c_str());
        if (filters.voiceType || filters.unknownVoiceTypeOnly) addAdvanced(
            TranslateText("Voice type"), filters.unknownVoiceTypeOnly ? TranslateText("Unknown") : voiceTypeFilterLabel_.c_str());
        if (filters.combatStyle || filters.unknownCombatStyleOnly) addAdvanced(
            TranslateText("Combat style"), filters.unknownCombatStyleOnly ? TranslateText("Unknown") : combatStyleFilterLabel_.c_str());
        if (filters.levelScaled != KnownBooleanFilter::Any) addKnown(TranslateText("Level scaled"), filters.levelScaled);
        const auto advancedCount = ActiveAdvancedSearchFilterCount(filters);
        const auto advancedBase = TranslateOwned("Advanced filters");
        const auto advancedCountHeading = advancedCount == 0 ? advancedBase :
            TranslateFormat("Advanced filters ({} active)", advancedCount);
        const auto advancedHeading = BuildFilterHeading(
            advancedCountHeading, advancedEntries, settings_.showActiveFilterNames, filterWidth,
            [&](std::string_view names) {
                return TranslateFormat("{} ({} active: {})", advancedBase, advancedCount, names);
            },
            [](std::string_view text) {
                return ImGuiMCP::CalcTextSize(text.data(), text.data() + text.size()).x;
            });
        const auto advancedHeadingId = std::format(
            "{}###WhereaboutsAdvancedFilters", advancedHeading.heading);
        const bool advancedOpen = ImGuiMCP::CollapsingHeader(advancedHeadingId.c_str());
        if (ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_DelayNormal)) {
            const auto tooltip = advancedHeading.tooltip.empty() ?
                TranslateOwned("No advanced filters active.") : advancedHeading.tooltip;
            ImGuiMCP::SetTooltip("%s", tooltip.c_str());
        }
        if (!advancedOpen) return;

        const int pushedStyle = pushDensityStyle(advancedDensity);
        const std::array sexNames{TranslateText("Any"), TranslateText("Male"),
            TranslateText("Female"), TranslateText("Unknown")};
        const std::array knownStates{TranslateText("Any"), TranslateText("Yes"),
            TranslateText("No"), TranslateText("Unknown")};
        const std::array areaNames{TranslateText("Any"), TranslateText("Interior"),
            TranslateText("Exterior"), TranslateText("Unknown")};
        const std::array freshnessNames{TranslateText("Any"), TranslateText("Current"),
            TranslateText("Last observed"), TranslateText("Unavailable")};
        const auto recordView = index_.Snapshot();
        if (recordView && recordView->catalog &&
            (recordOptionsSession_ != recordView->session || recordOptionsRevision_ != recordView->revision)) {
            factionFilterOptions_ = RecordFilterOptions(*recordView->catalog, RecordFacetCategory::Faction);
            keywordFilterOptions_ = RecordFilterOptions(*recordView->catalog, RecordFacetCategory::Keyword);
            classFilterOptions_ = RecordFilterOptions(*recordView->catalog, RecordFacetCategory::NpcClass);
            voiceTypeFilterOptions_ = RecordFilterOptions(*recordView->catalog, RecordFacetCategory::VoiceType);
            combatStyleFilterOptions_ = RecordFilterOptions(*recordView->catalog, RecordFacetCategory::CombatStyle);
            recordOptionsSession_ = recordView->session;
            recordOptionsRevision_ = recordView->revision;
            factionOptionResultsDirty_ = keywordOptionResultsDirty_ = classOptionResultsDirty_ =
                voiceTypeOptionResultsDirty_ = combatStyleOptionResultsDirty_ = true;
        }
        const auto recordFilter = [&]<std::size_t Size>(
            const char* comboId, const char* searchId, const char* label,
            std::optional<RecordFilterSelection>& selected, bool& unknownOnly,
            std::string& selectedLabel, std::array<char, Size>& search,
            const std::vector<RecordFilterOption>& options, RecordOptionMatches& visible,
            bool& dirty) {
            bool changed = false;
            const auto* value = unknownOnly ? TranslateText("Unknown") :
                selected ? selectedLabel.c_str() : TranslateText("Any");
            const auto preview = TriStateFilterPreview(label, value);
            const auto layout = ConstrainRecordPickerLayout(
                ImGuiMCP::GetContentRegionAvail().x, ImGuiMCP::GetFrameHeightWithSpacing());
            ImGuiMCP::SetNextItemWidth(layout.controlWidth);
            ImGuiMCP::SetNextWindowSizeConstraints(
                {layout.popupMinWidth, 0.0F}, {layout.popupMaxWidth, layout.popupMaxHeight});
            if (ImGuiMCP::BeginCombo(comboId, preview.c_str())) {
                if (ImGuiMCP::Selectable(TranslateText("Any"), !selected && !unknownOnly)) {
                    selected.reset(); unknownOnly = false; selectedLabel.clear(); changed = true;
                }
                if (ImGuiMCP::Selectable(TranslateText("Unknown"), unknownOnly)) {
                    selected.reset(); unknownOnly = true; selectedLabel.clear(); changed = true;
                }
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::IsWindowAppearing()) ImGuiMCP::SetKeyboardFocusHere();
                if (ImGuiMCP::InputText(searchId, search.data(), search.size())) dirty = true;
                if (dirty) { visible = FilterRecordOptions(options, search.data(), 50); dirty = false; }
                for (const auto index : visible.indices) {
                    const auto& option = options[index];
                    const float width = ImGuiMCP::GetContentRegionAvail().x;
                    if (ImGuiMCP::Selectable(option.label.c_str(), selected &&
                            selected->runtimeFormID == option.selection.runtimeFormID)) {
                        selected = option.selection; unknownOnly = false;
                        selectedLabel = option.label; changed = true;
                    }
                    OverflowTooltip(option.label, width);
                }
                if (visible.total == 0) ImGuiMCP::TextDisabled("%s", TranslateText("No matching records."));
                else if (visible.total > visible.indices.size()) {
                    const auto text = TranslateFormat("{} matches; type to narrow.", visible.total);
                    ImGuiMCP::TextDisabled("%s", text.c_str());
                }
                ImGuiMCP::EndCombo();
            }
            return changed;
        };
        const auto multiPlugin = [&]<std::size_t Size>(
            const char* comboId, const char* searchId, const char* label,
            std::vector<std::string>& selected, std::array<char, Size>& search,
            const std::vector<std::string>& options) {
            bool changed = false;
            const auto value = selected.empty() ? TranslateOwned("Any") : selected.size() == 1 ?
                selected.front() : TranslateFormat("{} selected", selected.size());
            const auto preview = TriStateFilterPreview(label, value.c_str());
            const auto layout = ConstrainRecordPickerLayout(
                ImGuiMCP::GetContentRegionAvail().x, ImGuiMCP::GetFrameHeightWithSpacing());
            ImGuiMCP::SetNextItemWidth(layout.controlWidth);
            ImGuiMCP::SetNextWindowSizeConstraints(
                {layout.popupMinWidth, 0.0F}, {layout.popupMaxWidth, layout.popupMaxHeight});
            if (ImGuiMCP::BeginCombo(comboId, preview.c_str())) {
                if (ImGuiMCP::Selectable(TranslateText("Any"), selected.empty())) {
                    selected.clear(); changed = true;
                }
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::IsWindowAppearing()) ImGuiMCP::SetKeyboardFocusHere();
                static_cast<void>(ImGuiMCP::InputText(searchId, search.data(), search.size()));
                const auto matches = FilterTextPickerOptions(options, search.data(), 50);
                for (const auto index : matches.indices) {
                    const auto& option = options[index];
                    const bool active = std::ranges::any_of(selected, [&](const auto& value) {
                        return SearchTextEqualsNoexcept(value, option);
                    });
                    const float width = ImGuiMCP::GetContentRegionAvail().x;
                    if (ImGuiMCP::Selectable(option.c_str(), active,
                            ImGuiMCP::ImGuiSelectableFlags_DontClosePopups)) {
                        if (active) std::erase_if(selected, [&](const auto& value) {
                            return SearchTextEqualsNoexcept(value, option);
                        });
                        else selected.push_back(option);
                        changed = true;
                    }
                    OverflowTooltip(option, width);
                }
                ImGuiMCP::EndCombo();
            }
            return changed;
        };
        std::vector<std::string> touchingOptions, originalOptions, winningOptions;
        if (recordView && recordView->catalog) {
            touchingOptions = ProvenancePluginOptions(*recordView->catalog, ProvenancePluginRole::Touching);
            originalOptions = ProvenancePluginOptions(*recordView->catalog, ProvenancePluginRole::Original);
            winningOptions = ProvenancePluginOptions(*recordView->catalog, ProvenancePluginRole::Winning);
        }
        const auto knownState = [&](const char* id, const char* label, int& value) {
            bool changed = false;
            const auto preview = TriStateFilterPreview(label, knownStates[std::clamp(value, 0, 3)]);
            ImGuiMCP::SetNextItemWidth(-1.0F);
            if (ImGuiMCP::BeginCombo(id, preview.c_str())) {
                for (int index = 0; index < 4; ++index) {
                    if (ImGuiMCP::Selectable(knownStates[index], value == index)) {
                        value = index; changed = true;
                    }
                }
                ImGuiMCP::EndCombo();
            }
            return changed;
        };
        const auto countPicker = [&](const char* id, const char* label, int& value) {
            bool changed = false;
            const auto shown = value == 0 ? TranslateOwned("Any") : std::to_string(value);
            const auto preview = TriStateFilterPreview(label, shown.c_str());
            ImGuiMCP::SetNextItemWidth(-1.0F);
            if (ImGuiMCP::BeginCombo(id, preview.c_str())) {
                if (ImGuiMCP::Selectable(TranslateText("Any"), value == 0)) { value = 0; changed = true; }
                for (int count = 1; count <= 10; ++count) {
                    const auto text = std::to_string(count);
                    if (ImGuiMCP::Selectable(text.c_str(), value == count)) { value = count; changed = true; }
                }
                ImGuiMCP::EndCombo();
            }
            return changed;
        };
        const auto renderAdvancedControl = [&](FilterControl control) {
            switch (control) {
            case FilterControl::Race: {
                const auto value = unknownRaceOnly_ ? TranslateText("Unknown") :
                    raceFilter_.empty() ? TranslateText("Any") : raceFilter_.c_str();
                const auto preview = TriStateFilterPreview(TranslateText("Race"), value);
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsRaceFilter", preview.c_str())) {
                    if (ImGuiMCP::Selectable(TranslateText("Any"), raceFilter_.empty() && !unknownRaceOnly_)) {
                        raceFilter_.clear(); unknownRaceOnly_ = false; searchOptionsChanged = true;
                    }
                    if (ImGuiMCP::Selectable(TranslateText("Unknown"), unknownRaceOnly_)) {
                        raceFilter_.clear(); unknownRaceOnly_ = true; searchOptionsChanged = true;
                    }
                    if (recordView && recordView->catalog) for (const auto& race : DemographicRaceOptions(*recordView->catalog)) {
                        if (ImGuiMCP::Selectable(race.c_str(), !unknownRaceOnly_ && SearchTextEqualsNoexcept(raceFilter_, race))) {
                            raceFilter_ = race; unknownRaceOnly_ = false; searchOptionsChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Select an observed NPC race."));
                break;
            }
            case FilterControl::Sex: {
                const auto preview = TriStateFilterPreview(TranslateText("Sex"), sexNames[std::clamp(sexFilter_, 0, 3)]);
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsSexFilter", preview.c_str())) {
                    for (int index = 0; index < 4; ++index) if (ImGuiMCP::Selectable(sexNames[index], sexFilter_ == index)) {
                        sexFilter_ = index; searchOptionsChanged = true;
                    }
                    ImGuiMCP::EndCombo();
                }
                break;
            }
            case FilterControl::Essential:
                searchOptionsChanged |= knownState("##WhereaboutsEssentialFilter", TranslateText("Essential"), essentialFilter_);
                DelayedTooltip(TranslateText("No means the flag is known to be absent."));
                break;
            case FilterControl::Protected:
                searchOptionsChanged |= knownState("##WhereaboutsProtectedFilter", TranslateText("Protected"), protectedFilter_);
                DelayedTooltip(TranslateText("No means the flag is known to be absent."));
                break;
            case FilterControl::Area: {
                const auto preview = TriStateFilterPreview(TranslateText("Area"), areaNames[std::clamp(spatialKindFilter_, 0, 3)]);
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsAreaFilter", preview.c_str())) {
                    for (int index = 0; index < 4; ++index) if (ImGuiMCP::Selectable(areaNames[index], spatialKindFilter_ == index)) {
                        spatialKindFilter_ = index; searchOptionsChanged = true;
                    }
                    ImGuiMCP::EndCombo();
                }
                break;
            }
            case FilterControl::LocationData: {
                const auto preview = TriStateFilterPreview(
                    densityLabel(advancedDensity, "Location data", "Location data", "Location"),
                    freshnessNames[std::clamp(spatialFreshnessFilter_, 0, 3)]);
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsSpatialFreshnessFilter", preview.c_str())) {
                    for (int index = 0; index < 4; ++index) if (ImGuiMCP::Selectable(freshnessNames[index], spatialFreshnessFilter_ == index)) {
                        spatialFreshnessFilter_ = index; searchOptionsChanged = true;
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Current is loaded; Last observed is unloaded."));
                break;
            }
            case FilterControl::Worldspace: {
                const auto* value = unknownWorldspaceOnly_ ? TranslateText("Unknown") :
                    worldspaceFilterFormID_ == 0 ? TranslateText("Any") : worldspaceFilterLabel_.c_str();
                const auto preview = TriStateFilterPreview(TranslateText("Worldspace"), value);
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::BeginCombo("##WhereaboutsWorldspaceFilter", preview.c_str())) {
                    if (ImGuiMCP::Selectable(TranslateText("Any"), worldspaceFilterFormID_ == 0 && !unknownWorldspaceOnly_)) {
                        worldspaceFilterFormID_ = 0; unknownWorldspaceOnly_ = false;
                        worldspaceFilterLabel_.clear(); searchOptionsChanged = true;
                    }
                    if (ImGuiMCP::Selectable(TranslateText("Unknown"), unknownWorldspaceOnly_)) {
                        worldspaceFilterFormID_ = 0; unknownWorldspaceOnly_ = true;
                        worldspaceFilterLabel_.clear(); searchOptionsChanged = true;
                    }
                    if (recordView && recordView->catalog) for (const auto& option : SpatialWorldspaceOptions(*recordView->catalog)) {
                        if (ImGuiMCP::Selectable(option.label.c_str(), !unknownWorldspaceOnly_ && worldspaceFilterFormID_ == option.formID)) {
                            worldspaceFilterFormID_ = option.formID; unknownWorldspaceOnly_ = false;
                            worldspaceFilterLabel_ = option.label; searchOptionsChanged = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                DelayedTooltip(TranslateText("Interior cells normally have no worldspace."));
                break;
            }
            case FilterControl::Faction: {
                const auto searchLabel = std::format("{}##WhereaboutsFactionOptionSearch", TranslateText("Find faction"));
                searchOptionsChanged |= recordFilter("##WhereaboutsFactionFilter", searchLabel.c_str(),
                    TranslateText("Faction"), factionFilter_, unknownFactionsOnly_, factionFilterLabel_,
                    factionOptionSearch_, factionFilterOptions_, visibleFactionOptions_, factionOptionResultsDirty_);
                DelayedTooltip(TranslateText("Filter by an NPC's base faction."));
                break;
            }
            case FilterControl::BaseKeyword: {
                const auto searchLabel = std::format("{}##WhereaboutsKeywordOptionSearch", TranslateText("Find keyword"));
                searchOptionsChanged |= recordFilter("##WhereaboutsBaseKeywordFilter", searchLabel.c_str(),
                    densityLabel(advancedDensity, "Base keyword", "Base keyword", "Keyword"), baseKeywordFilter_,
                    unknownBaseKeywordsOnly_, baseKeywordFilterLabel_, keywordOptionSearch_,
                    keywordFilterOptions_, visibleKeywordOptions_, keywordOptionResultsDirty_);
                DelayedTooltip(TranslateText("Filter by a keyword on the base NPC."));
                break;
            }
            case FilterControl::TouchesNpcRecord:
                searchOptionsChanged |= multiPlugin("##WhereaboutsTouchingPluginFilter", "##WhereaboutsTouchingPluginSearch",
                    densityLabel(advancedDensity, "Touches NPC record", "Touches record", "Touches"),
                    touchingPluginFilters_, touchingPluginSearch_, touchingOptions);
                DelayedTooltip(TranslateText("Touches NPC record"));
                break;
            case FilterControl::OriginalPlugin:
                searchOptionsChanged |= multiPlugin("##WhereaboutsOriginalPluginFilter", "##WhereaboutsOriginalPluginSearch",
                    densityLabel(advancedDensity, "Original plugin", "Original", "Original"),
                    originalPluginFilters_, originalPluginSearch_, originalOptions);
                DelayedTooltip(TranslateText("Original plugin"));
                break;
            case FilterControl::WinningPlugin:
                searchOptionsChanged |= multiPlugin("##WhereaboutsWinningPluginFilter", "##WhereaboutsWinningPluginSearch",
                    densityLabel(advancedDensity, "Winning plugin", "Winning", "Winner"),
                    winningPluginFilters_, winningPluginSearch_, winningOptions);
                DelayedTooltip(TranslateText("Winning plugin"));
                break;
            case FilterControl::Class: {
                const auto searchLabel = std::format("{}##WhereaboutsClassOptionSearch", TranslateText("Find class"));
                searchOptionsChanged |= recordFilter("##WhereaboutsClassFilter", searchLabel.c_str(), TranslateText("Class"),
                    classFilter_, unknownClassOnly_, classFilterLabel_, classOptionSearch_, classFilterOptions_,
                    visibleClassOptions_, classOptionResultsDirty_);
                break;
            }
            case FilterControl::VoiceType: {
                const auto searchLabel = std::format("{}##WhereaboutsVoiceOptionSearch", TranslateText("Find voice type"));
                searchOptionsChanged |= recordFilter("##WhereaboutsVoiceTypeFilter", searchLabel.c_str(),
                    densityLabel(advancedDensity, "Voice type", "Voice", "Voice"), voiceTypeFilter_, unknownVoiceTypeOnly_,
                    voiceTypeFilterLabel_, voiceTypeOptionSearch_, voiceTypeFilterOptions_,
                    visibleVoiceTypeOptions_, voiceTypeOptionResultsDirty_);
                DelayedTooltip(TranslateText("Voice type"));
                break;
            }
            case FilterControl::CombatStyle: {
                const auto searchLabel = std::format("{}##WhereaboutsCombatOptionSearch", TranslateText("Find combat style"));
                searchOptionsChanged |= recordFilter("##WhereaboutsCombatStyleFilter", searchLabel.c_str(),
                    densityLabel(advancedDensity, "Combat style", "Combat", "Combat"), combatStyleFilter_, unknownCombatStyleOnly_,
                    combatStyleFilterLabel_, combatStyleOptionSearch_, combatStyleFilterOptions_,
                    visibleCombatStyleOptions_, combatStyleOptionResultsDirty_);
                DelayedTooltip(TranslateText("Combat style"));
                break;
            }
            case FilterControl::ConflictedRecord:
                searchOptionsChanged |= knownState("##WhereaboutsMultiplePluginRecords",
                    densityLabel(advancedDensity, "Multiple plugin records", "Conflicted", "Conflict"), multiplePluginRecordsFilter_);
                DelayedTooltip(TranslateText("Multiple plugin records"));
                break;
            case FilterControl::LevelScaled:
                searchOptionsChanged |= knownState("##WhereaboutsLevelScaling",
                    densityLabel(advancedDensity, "Level scaled", "Level scaled", "Scaled"), levelScalingFilter_);
                DelayedTooltip(TranslateText("Level scaled"));
                break;
            case FilterControl::MinimumPluginRecords:
                searchOptionsChanged |= countPicker("##WhereaboutsMinimumPluginRecordCount",
                    densityLabel(advancedDensity, "Min plugin records", "Min records", "Min"), minimumPluginRecordCount_);
                DelayedTooltip(TranslateText("Min plugin records"));
                break;
            case FilterControl::MaximumPluginRecords:
                searchOptionsChanged |= countPicker("##WhereaboutsMaximumPluginRecordCount",
                    densityLabel(advancedDensity, "Max plugin records", "Max records", "Max"), maximumPluginRecordCount_);
                DelayedTooltip(TranslateText("Max plugin records"));
                break;
            case FilterControl::ClearAdvanced: {
                const auto label = std::format(
                    "{}##WhereaboutsClearAdvancedFilters", TranslateText("Clear Filters"));
                if (ImGuiMCP::Button(label.c_str(), {-1.0F, 0.0F})) {
                    ResetFiltersToDefaults(); searchOptionsChanged = true; catalogModeChanged = true;
                }
                break;
            }
            default: break;
            }
        };
        renderRows("WhereaboutsAdvancedFiltersGrid",
            BuildAdvancedFilterLayout(advancedDensity, filterWidth, filterSpacing),
            renderAdvancedControl);
        if (pushedStyle > 0) ImGuiMCP::PopStyleVar(pushedStyle);
    }
}
