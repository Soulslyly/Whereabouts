#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace whereabouts
{
    enum class MovementBoundary
    {
        SameArea,
        DifferentCell,
        DifferentWorldspace,
        Unknown
    };

    enum class SpatialKind
    {
        Unknown,
        Interior,
        Exterior
    };

    enum class SpatialFreshness
    {
        Current,
        LastObserved,
        Unavailable
    };

    enum class LocationSource
    {
        None,
        Actor,
        Cell,
        NamedParent
    };

    struct SpatialSnapshot
    {
        std::string location;
        std::string cell;
        std::string worldspace;
        std::uint32_t locationFormID{0};
        std::uint32_t cellFormID{0};
        std::uint32_t worldspaceFormID{0};
        std::string locationEditorID;
        std::string cellEditorID;
        std::string worldspaceEditorID;
        std::optional<std::int32_t> exteriorCellX;
        std::optional<std::int32_t> exteriorCellY;
        LocationSource locationSource{LocationSource::None};
        SpatialKind kind{SpatialKind::Unknown};
        SpatialFreshness freshness{SpatialFreshness::Unavailable};
        bool sameCell{false};
        bool sameLocation{false};
        bool sameWorldspace{false};
        MovementBoundary movementBoundary{MovementBoundary::Unknown};
        std::optional<float> distance;

        [[nodiscard]] bool operator==(const SpatialSnapshot&) const noexcept = default;
    };
}
