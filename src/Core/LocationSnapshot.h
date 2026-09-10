#pragma once

#include "Core/FormIdentity.h"

#include <cstdint>
#include <string>

namespace whereabouts
{
    struct LocationSnapshot
    {
        std::string displayName;
        std::string editorID;
        std::uint32_t runtimeFormID{0};
        FormIdentity identity;
        std::string containingLocation;
        std::string worldspace;
        bool interior{false};
        bool hasExteriorGrid{false};
        std::int32_t exteriorCellX{0};
        std::int32_t exteriorCellY{0};

        std::string searchName;
        std::string searchEditorID;
        std::string searchPlugin;
        std::string searchContext;
        std::string searchWorldspace;

        [[nodiscard]] std::string_view SourcePlugin() const noexcept
        {
            return identity.plugin;
        }
    };

    [[nodiscard]] constexpr bool IsLocationCatalogCandidate(
        bool deleted,
        bool stableIdentity,
        bool hasName,
        bool hasEditorID) noexcept
    {
        return !deleted && stableIdentity && (hasName || hasEditorID);
    }
}
