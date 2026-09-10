#pragma once

#include "Compatibility/MenuFrameworkCompatibility.h"

#include <optional>
#include <string>

namespace whereabouts
{
    struct MenuFrameworkRuntimeProbe
    {
        bool moduleLoaded{false};
        std::optional<MenuFrameworkVersion> fixedVersion;
        bool requiredExportsAvailable{false};
        std::string missingRequiredExport;
    };

    [[nodiscard]] MenuFrameworkRuntimeProbe ProbeLoadedMenuFramework() noexcept;
}
