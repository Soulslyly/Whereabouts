#pragma once

#include "Core/NpcSnapshot.h"
#include "Core/RuntimeIndexSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace whereabouts
{
    enum class SortKey
    {
        Name,
        Plugin,
        Level,
        Location,
        Loaded,
        Distance,
        Status,
        Random
    };

    enum class KnownBooleanFilter
    {
        Any,
        Yes,
        No,
        Unknown
    };

    struct SearchFilters
    {
        std::optional<std::string> plugin;
        std::vector<std::string> selectedPlugins;
        std::optional<std::uint16_t> minimumLevel;
        std::optional<std::uint16_t> maximumLevel;
        std::optional<bool> alive;
        std::optional<bool> enabled;
        std::optional<bool> teammate;
        std::optional<bool> potentialFollower;
        std::optional<bool> loaded;
        std::optional<std::string> location;
        bool locationExact{false};
        std::optional<std::string> race;
        bool unknownRaceOnly{false};
        std::optional<NpcSex> sex;
        KnownBooleanFilter essential{KnownBooleanFilter::Any};
        KnownBooleanFilter protectedActor{KnownBooleanFilter::Any};
        std::optional<SpatialKind> spatialKind;
        std::optional<SpatialFreshness> spatialFreshness;
        std::optional<std::uint32_t> worldspaceFormID;
        bool unknownWorldspaceOnly{false};
        std::optional<RecordFilterSelection> faction;
        bool unknownFactionsOnly{false};
        std::optional<RecordFilterSelection> baseKeyword;
        bool unknownBaseKeywordsOnly{false};
        bool favoritesOnly{false};
        bool trackedOnly{false};
        bool sameLocationOnly{false};
        bool includeGeneric{false};
        bool genericOnly{false};
    };

    struct SearchQuery
    {
        std::string text;
        SearchFilters filters;
        SortKey sort{SortKey::Name};
        bool ascending{true};
        std::uint64_t randomSeed{0};
        std::size_t limit{10};
        bool unlimitedResults{false};
        std::vector<FormIdentity> favoriteIdentities;
    };

    [[nodiscard]] constexpr std::size_t ActiveSearchFilterCount(
        const SearchFilters& filters) noexcept
    {
        std::size_t count = 0;
        count += filters.plugin.has_value() || !filters.selectedPlugins.empty();
        count += filters.minimumLevel.has_value();
        count += filters.maximumLevel.has_value();
        count += filters.alive.has_value();
        count += filters.enabled.has_value();
        count += filters.teammate.has_value();
        count += filters.potentialFollower.has_value();
        count += filters.loaded.has_value();
        count += filters.location.has_value();
        count += filters.race.has_value() || filters.unknownRaceOnly;
        count += filters.sex.has_value();
        count += filters.essential != KnownBooleanFilter::Any;
        count += filters.protectedActor != KnownBooleanFilter::Any;
        count += filters.spatialKind.has_value();
        count += filters.spatialFreshness.has_value();
        count += filters.worldspaceFormID.has_value() || filters.unknownWorldspaceOnly;
        count += filters.faction.has_value() || filters.unknownFactionsOnly;
        count += filters.baseKeyword.has_value() || filters.unknownBaseKeywordsOnly;
        count += filters.favoritesOnly;
        count += filters.trackedOnly;
        count += filters.sameLocationOnly;
        count += filters.includeGeneric;
        count += filters.genericOnly;
        return count;
    }

    [[nodiscard]] constexpr bool HasActiveSearchFilters(
        const SearchFilters& filters) noexcept
    {
        return ActiveSearchFilterCount(filters) != 0;
    }

    [[nodiscard]] constexpr std::size_t ActiveAdvancedSearchFilterCount(
        const SearchFilters& filters) noexcept
    {
        std::size_t count = 0;
        count += filters.race.has_value() || filters.unknownRaceOnly;
        count += filters.sex.has_value();
        count += filters.essential != KnownBooleanFilter::Any;
        count += filters.protectedActor != KnownBooleanFilter::Any;
        count += filters.spatialKind.has_value();
        count += filters.spatialFreshness.has_value();
        count += filters.worldspaceFormID.has_value() || filters.unknownWorldspaceOnly;
        count += filters.faction.has_value() || filters.unknownFactionsOnly;
        count += filters.baseKeyword.has_value() || filters.unknownBaseKeywordsOnly;
        return count;
    }

    [[nodiscard]] constexpr std::size_t ActiveCommonSearchFilterCount(
        const SearchFilters& filters) noexcept
    {
        std::size_t count = 0;
        count += filters.plugin.has_value() || !filters.selectedPlugins.empty();
        count += filters.minimumLevel.has_value();
        count += filters.maximumLevel.has_value();
        count += filters.alive.has_value();
        count += filters.enabled.has_value();
        count += filters.teammate.has_value();
        count += filters.potentialFollower.has_value();
        count += filters.loaded.has_value();
        count += filters.location.has_value();
        count += filters.favoritesOnly;
        count += filters.trackedOnly;
        count += filters.sameLocationOnly;
        count += filters.includeGeneric;
        count += filters.genericOnly;
        return count;
    }

    constexpr void ClearAdvancedSearchFilters(SearchFilters& filters) noexcept
    {
        filters.race.reset();
        filters.unknownRaceOnly = false;
        filters.sex.reset();
        filters.essential = KnownBooleanFilter::Any;
        filters.protectedActor = KnownBooleanFilter::Any;
        filters.spatialKind.reset();
        filters.spatialFreshness.reset();
        filters.worldspaceFormID.reset();
        filters.unknownWorldspaceOnly = false;
        filters.faction.reset();
        filters.unknownFactionsOnly = false;
        filters.baseKeyword.reset();
        filters.unknownBaseKeywordsOnly = false;
    }

    struct SearchError
    {
        std::string message;
    };

    struct SearchMatches
    {
        std::vector<NpcSnapshot> visible;
        std::size_t total{0};
        std::size_t textMatchTotal{0};
    };

    using SearchResult = std::variant<SearchMatches, SearchError>;

    [[nodiscard]] SearchResult Search(
        RuntimeIndexView source,
        const SearchQuery& query);

    [[nodiscard]] std::vector<std::string> SuggestNpcNames(
        RuntimeIndexView source,
        std::string_view text,
        std::size_t limit = 3);
}
