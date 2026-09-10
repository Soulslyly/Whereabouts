#pragma once

#include <string>

namespace whereabouts
{
    enum class DistanceUnit
    {
        Meters,
        Feet,
        GameUnits
    };

    [[nodiscard]] std::string FormatDistance(float gameUnits, DistanceUnit unit);
}
