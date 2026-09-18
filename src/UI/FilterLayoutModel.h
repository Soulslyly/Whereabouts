#pragma once

#include "Persistence/Settings.h"

#include <algorithm>
#include <span>
#include <vector>

namespace whereabouts::ui
{
    enum class FilterControl
    {
        Plugin,
        Location,
        SearchContent,
        ResultSectionOrder,
        Alive,
        Enabled,
        Loaded,
        Follower,
        PotentialFollower,
        FavoritesOnly,
        TrackedOnly,
        SameLocation,
        IncludeGeneric,
        Sort,
        Direction,
        ClearCommon,
        Race,
        Sex,
        Essential,
        Protected,
        Area,
        LocationData,
        Worldspace,
        LevelScaled,
        MinimumPluginRecords,
        MaximumPluginRecords,
        Faction,
        BaseKeyword,
        TouchesNpcRecord,
        OriginalPlugin,
        WinningPlugin,
        ConflictedRecord,
        Class,
        VoiceType,
        CombatStyle,
        ClearAdvanced
    };

    struct FilterCell
    {
        FilterControl control{};
        float weight{1.0F};

        friend constexpr bool operator==(const FilterCell&, const FilterCell&) = default;
    };

    struct FilterRow
    {
        std::vector<FilterCell> cells;
    };

    struct FilterLayoutPlan
    {
        std::vector<FilterRow> rows;
    };

    namespace detail
    {
        inline constexpr float kMinimumFilterWidthUnit{100.0F};

        [[nodiscard]] inline FilterLayoutPlan WrapFilterRows(
            std::span<const FilterRow> templates,
            float availableWidth,
            float itemSpacing)
        {
            FilterLayoutPlan result;
            const float safeWidth = (std::max)(1.0F, availableWidth);
            const float safeSpacing = (std::max)(0.0F, itemSpacing);

            for (const auto& rowTemplate : templates) {
                FilterRow current;
                float usedWidth = 0.0F;
                for (const auto& cell : rowTemplate.cells) {
                    const float cellWidth =
                        (std::max)(cell.weight, 0.01F) * kMinimumFilterWidthUnit;
                    const float candidate = usedWidth +
                        (current.cells.empty() ? 0.0F : safeSpacing) + cellWidth;
                    if (!current.cells.empty() && candidate > safeWidth) {
                        result.rows.push_back(std::move(current));
                        current = {};
                        usedWidth = 0.0F;
                    }
                    if (!current.cells.empty()) usedWidth += safeSpacing;
                    current.cells.push_back(cell);
                    usedWidth += cellWidth;
                }
                if (!current.cells.empty()) result.rows.push_back(std::move(current));
            }
            return result;
        }

        [[nodiscard]] inline FilterRow Row(std::initializer_list<FilterCell> cells)
        {
            return {std::vector<FilterCell>{cells}};
        }

        [[nodiscard]] inline FilterCell Cell(FilterControl control, float weight = 1.0F)
        {
            return {control, weight};
        }
    }

    [[nodiscard]] inline FilterLayoutPlan BuildCommonFilterLayout(
        ResultDensity density,
        bool includeResultSectionOrder,
        float availableWidth,
        float itemSpacing)
    {
        using detail::Cell;
        using detail::Row;
        std::vector<FilterRow> rows;

        if (density == ResultDensity::Detailed) {
            rows = {
                Row({Cell(FilterControl::Plugin, 2.0F), Cell(FilterControl::Location, 2.0F)}),
                Row({Cell(FilterControl::SearchContent), Cell(FilterControl::Alive),
                    Cell(FilterControl::Enabled)}),
                Row({Cell(FilterControl::Follower), Cell(FilterControl::PotentialFollower),
                    Cell(FilterControl::Loaded)}),
                Row({Cell(FilterControl::FavoritesOnly), Cell(FilterControl::TrackedOnly),
                    Cell(FilterControl::SameLocation), Cell(FilterControl::IncludeGeneric)}),
                Row({Cell(FilterControl::Sort), Cell(FilterControl::Direction),
                    Cell(FilterControl::ClearCommon)})};
        } else {
            rows = {
                Row({Cell(FilterControl::Plugin), Cell(FilterControl::Location)}),
                Row({Cell(FilterControl::SearchContent), Cell(FilterControl::Alive),
                    Cell(FilterControl::Enabled),
                    Cell(FilterControl::Follower), Cell(FilterControl::PotentialFollower),
                    Cell(FilterControl::Loaded)}),
                Row({Cell(FilterControl::FavoritesOnly), Cell(FilterControl::TrackedOnly),
                    Cell(FilterControl::SameLocation), Cell(FilterControl::IncludeGeneric)}),
                Row({Cell(FilterControl::Sort), Cell(FilterControl::Direction),
                    Cell(FilterControl::ClearCommon)})};
        }

        if (includeResultSectionOrder) {
            for (auto& row : rows) {
                const auto position = std::ranges::find(
                    row.cells, FilterControl::SearchContent, &FilterCell::control);
                if (position != row.cells.end()) {
                    row.cells.insert(std::next(position), Cell(FilterControl::ResultSectionOrder));
                    break;
                }
            }
        }
        return detail::WrapFilterRows(rows, availableWidth, itemSpacing);
    }

    [[nodiscard]] inline FilterLayoutPlan BuildAdvancedFilterLayout(
        ResultDensity density,
        float availableWidth,
        float itemSpacing)
    {
        using detail::Cell;
        using detail::Row;
        std::vector<FilterRow> rows;

        if (density == ResultDensity::Detailed) {
            rows = {
                Row({Cell(FilterControl::Race), Cell(FilterControl::Sex),
                    Cell(FilterControl::Essential), Cell(FilterControl::Protected)}),
                Row({Cell(FilterControl::Area), Cell(FilterControl::LocationData),
                    Cell(FilterControl::Worldspace), Cell(FilterControl::LevelScaled),
                    Cell(FilterControl::MinimumPluginRecords),
                    Cell(FilterControl::MaximumPluginRecords)}),
                Row({Cell(FilterControl::Faction, 2.0F), Cell(FilterControl::BaseKeyword, 2.0F)}),
                Row({Cell(FilterControl::TouchesNpcRecord), Cell(FilterControl::OriginalPlugin),
                    Cell(FilterControl::WinningPlugin), Cell(FilterControl::ConflictedRecord)}),
                Row({Cell(FilterControl::Class), Cell(FilterControl::VoiceType),
                    Cell(FilterControl::CombatStyle), Cell(FilterControl::ClearAdvanced)})};
        } else if (density == ResultDensity::Compact) {
            rows = {
                Row({Cell(FilterControl::Race), Cell(FilterControl::Sex),
                    Cell(FilterControl::Essential), Cell(FilterControl::Protected),
                    Cell(FilterControl::Area), Cell(FilterControl::LocationData)}),
                Row({Cell(FilterControl::Worldspace), Cell(FilterControl::LevelScaled),
                    Cell(FilterControl::ConflictedRecord),
                    Cell(FilterControl::MinimumPluginRecords),
                    Cell(FilterControl::MaximumPluginRecords)}),
                Row({Cell(FilterControl::Faction, 2.0F), Cell(FilterControl::BaseKeyword, 2.0F),
                    Cell(FilterControl::TouchesNpcRecord)}),
                Row({Cell(FilterControl::OriginalPlugin), Cell(FilterControl::WinningPlugin),
                    Cell(FilterControl::Class), Cell(FilterControl::VoiceType),
                    Cell(FilterControl::CombatStyle), Cell(FilterControl::ClearAdvanced)})};
        } else {
            rows = {
                Row({Cell(FilterControl::Race), Cell(FilterControl::Sex),
                    Cell(FilterControl::Essential), Cell(FilterControl::Protected),
                    Cell(FilterControl::Area), Cell(FilterControl::LocationData),
                    Cell(FilterControl::Worldspace), Cell(FilterControl::LevelScaled)}),
                Row({Cell(FilterControl::Faction, 2.0F), Cell(FilterControl::BaseKeyword, 2.0F),
                    Cell(FilterControl::TouchesNpcRecord), Cell(FilterControl::OriginalPlugin),
                    Cell(FilterControl::WinningPlugin), Cell(FilterControl::ConflictedRecord)}),
                Row({Cell(FilterControl::Class, 2.0F), Cell(FilterControl::VoiceType, 2.0F),
                    Cell(FilterControl::CombatStyle, 2.0F),
                    Cell(FilterControl::MinimumPluginRecords),
                    Cell(FilterControl::MaximumPluginRecords), Cell(FilterControl::ClearAdvanced)})};
        }
        return detail::WrapFilterRows(rows, availableWidth, itemSpacing);
    }
}
