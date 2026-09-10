#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace whereabouts
{
    struct MenuFrameworkVersion
    {
        std::uint16_t major{0};
        std::uint16_t minor{0};
        std::uint16_t patch{0};
        std::uint16_t build{0};

        friend constexpr bool operator==(const MenuFrameworkVersion&, const MenuFrameworkVersion&) = default;
    };

    enum class MenuFrameworkCompatibility
    {
        Compatible,
        ModuleUnavailable,
        VersionUnavailable,
        UnsupportedVersion,
        MissingRequiredExport
    };

    [[nodiscard]] constexpr bool SupportsMenuFrameworkVersion(
        MenuFrameworkVersion version) noexcept
    {
        return version.major == 3 && version.minor >= 14;
    }

    [[nodiscard]] constexpr MenuFrameworkCompatibility DecideMenuFrameworkCompatibility(
        bool moduleLoaded,
        std::optional<MenuFrameworkVersion> fixedVersion,
        bool requiredExportsAvailable) noexcept
    {
        if (!moduleLoaded) return MenuFrameworkCompatibility::ModuleUnavailable;
        if (!fixedVersion) return MenuFrameworkCompatibility::VersionUnavailable;
        if (!SupportsMenuFrameworkVersion(*fixedVersion)) {
            return MenuFrameworkCompatibility::UnsupportedVersion;
        }
        if (!requiredExportsAvailable) {
            return MenuFrameworkCompatibility::MissingRequiredExport;
        }
        return MenuFrameworkCompatibility::Compatible;
    }

    [[nodiscard]] constexpr std::string_view MenuFrameworkCompatibilityLabel(
        MenuFrameworkCompatibility compatibility) noexcept
    {
        switch (compatibility) {
        case MenuFrameworkCompatibility::Compatible: return "compatible";
        case MenuFrameworkCompatibility::ModuleUnavailable: return "module unavailable";
        case MenuFrameworkCompatibility::VersionUnavailable: return "fixed version unavailable";
        case MenuFrameworkCompatibility::UnsupportedVersion: return "unsupported fixed version";
        case MenuFrameworkCompatibility::MissingRequiredExport: return "required export missing";
        }
        return "unknown";
    }
}
