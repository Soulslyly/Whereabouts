#pragma once

#include "Core/SpatialSnapshot.h"

#include <string>
#include <string_view>

namespace whereabouts
{
    [[nodiscard]] std::string PrimarySpatialLabel(const SpatialSnapshot& spatial);
    [[nodiscard]] std::string SecondaryWorldspaceLabel(const SpatialSnapshot& spatial);
    [[nodiscard]] std::string SearchableSpatialText(const SpatialSnapshot& spatial);
    [[nodiscard]] std::string PreferredWorldspaceName(
        std::string_view actorWorldspace,
        std::string_view cellWorldspace);
    [[nodiscard]] std::string WorldspaceSpatialLabel(const SpatialSnapshot& spatial);
    [[nodiscard]] std::string SpatialIdentityLabel(
        std::string_view editorID,
        std::uint32_t runtimeFormID);
    [[nodiscard]] std::string ExteriorCellGridLabel(const SpatialSnapshot& spatial);
    [[nodiscard]] std::string_view LocationSourceLabel(LocationSource source) noexcept;
    [[nodiscard]] std::string_view SpatialFreshnessLabel(SpatialFreshness freshness) noexcept;
}
