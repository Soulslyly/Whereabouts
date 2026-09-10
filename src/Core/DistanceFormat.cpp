#include "Core/DistanceFormat.h"

#include <cmath>
#include <format>

namespace whereabouts
{
    std::string FormatDistance(float gameUnits, DistanceUnit unit)
    {
        if (!std::isfinite(gameUnits) || gameUnits < 0.0F) return "Unknown";

        switch (unit) {
        case DistanceUnit::Meters:
            return std::format("{:.1f} m", gameUnits / 70.0F);
        case DistanceUnit::Feet:
            return std::format("{:.0f} ft", gameUnits * (6.0F / 128.0F));
        case DistanceUnit::GameUnits:
            return std::format("{:.0f} units", gameUnits);
        }
        return "Unknown";
    }
}
