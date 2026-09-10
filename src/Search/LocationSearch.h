#pragma once

#include "Core/LocationSnapshot.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace whereabouts
{
    enum class LocationSortKey
    {
        Name,
        Plugin,
        Worldspace
    };

    struct LocationSearchQuery
    {
        std::string text;
        std::optional<std::string> plugin;
        std::vector<std::string> selectedPlugins;
        std::optional<std::string> context;
        LocationSortKey sort{LocationSortKey::Name};
        bool ascending{true};
        std::size_t limit{10};
        bool unlimitedResults{false};
    };

    struct LocationSearchMatches
    {
        std::vector<LocationSnapshot> visible;
        std::size_t total{0};
    };

    [[nodiscard]] LocationSearchMatches SearchLocations(
        std::span<const LocationSnapshot> source,
        const LocationSearchQuery& query);

    [[nodiscard]] inline LocationSearchMatches SearchLocations(
        std::initializer_list<LocationSnapshot> source,
        const LocationSearchQuery& query)
    {
        return SearchLocations(std::span<const LocationSnapshot>{source.begin(), source.size()}, query);
    }
}
