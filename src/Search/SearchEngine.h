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

    [[nodiscard]] constexpr bool HasActiveSearchFilters(
        const SearchFilters& filters) noexcept
    {
        return filters.plugin.has_value() ||
            !filters.selectedPlugins.empty() ||
            filters.minimumLevel.has_value() ||
            filters.maximumLevel.has_value() ||
            filters.alive.has_value() ||
            filters.enabled.has_value() ||
            filters.teammate.has_value() ||
            filters.potentialFollower.has_value() ||
            filters.loaded.has_value() ||
            filters.location.has_value() ||
            filters.favoritesOnly ||
            filters.trackedOnly ||
            filters.sameLocationOnly ||
            filters.includeGeneric ||
            filters.genericOnly;
    }

    struct SearchError
    {
        std::string message;
    };

    struct SearchMatches
    {
        std::vector<NpcSnapshot> visible;
        std::size_t total{0};
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
