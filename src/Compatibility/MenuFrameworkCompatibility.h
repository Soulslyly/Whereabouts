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

    enum class MenuFrameworkProvider
    {
        Unknown,
        SkseMenuFramework,
        ApocryphaRealmMenuFramework
    };

    enum class MenuFrameworkCompatibility
    {
        Compatible,
        ModuleUnavailable,
        UnknownProvider,
        VersionUnavailable,
        UnsupportedVersion,
        MissingRequiredExport
    };

    [[nodiscard]] constexpr wchar_t FoldAscii(wchar_t value) noexcept
    {
        return value >= L'A' && value <= L'Z' ? value + (L'a' - L'A') : value;
    }

    [[nodiscard]] constexpr bool EqualsAsciiInsensitive(
        std::wstring_view left,
        std::wstring_view right) noexcept
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (FoldAscii(left[index]) != FoldAscii(right[index])) return false;
        }
        return true;
    }

    [[nodiscard]] constexpr MenuFrameworkProvider IdentifyMenuFrameworkProvider(
        std::wstring_view modulePath) noexcept
    {
        const auto separator = modulePath.find_last_of(L"\\/");
        const auto fileName = separator == std::wstring_view::npos ?
            modulePath :
            modulePath.substr(separator + 1);
        if (EqualsAsciiInsensitive(fileName, L"SKSEMenuFramework.dll")) {
            return MenuFrameworkProvider::SkseMenuFramework;
        }
        if (EqualsAsciiInsensitive(fileName, L"!ApocryphaMenuFramework.dll") ||
            EqualsAsciiInsensitive(fileName, L"ApocryphaMenuFramework.dll")) {
            return MenuFrameworkProvider::ApocryphaRealmMenuFramework;
        }
        return MenuFrameworkProvider::Unknown;
    }

    [[nodiscard]] constexpr bool SupportsMenuFrameworkVersion(
        MenuFrameworkProvider provider,
        MenuFrameworkVersion version) noexcept
    {
        switch (provider) {
        case MenuFrameworkProvider::SkseMenuFramework:
            return version.major == 3 && version.minor >= 14;
        case MenuFrameworkProvider::ApocryphaRealmMenuFramework:
            return version.major == 1 &&
                   (version.minor > 8 || (version.minor == 8 && version.patch >= 4));
        case MenuFrameworkProvider::Unknown:
            return false;
        }
        return false;
    }

    [[nodiscard]] constexpr MenuFrameworkCompatibility DecideMenuFrameworkCompatibility(
        bool moduleLoaded,
        MenuFrameworkProvider provider,
        std::optional<MenuFrameworkVersion> fixedVersion,
        bool requiredExportsAvailable) noexcept
    {
        if (!moduleLoaded) return MenuFrameworkCompatibility::ModuleUnavailable;
        if (provider == MenuFrameworkProvider::Unknown) {
            return MenuFrameworkCompatibility::UnknownProvider;
        }
        if (!fixedVersion) return MenuFrameworkCompatibility::VersionUnavailable;
        if (!SupportsMenuFrameworkVersion(provider, *fixedVersion)) {
            return MenuFrameworkCompatibility::UnsupportedVersion;
        }
        if (!requiredExportsAvailable) {
            return MenuFrameworkCompatibility::MissingRequiredExport;
        }
        return MenuFrameworkCompatibility::Compatible;
    }

    [[nodiscard]] constexpr std::string_view MenuFrameworkProviderLabel(
        MenuFrameworkProvider provider) noexcept
    {
        switch (provider) {
        case MenuFrameworkProvider::SkseMenuFramework: return "SKSE Menu Framework";
        case MenuFrameworkProvider::ApocryphaRealmMenuFramework:
            return "ApocryphaRealm Menu Framework";
        case MenuFrameworkProvider::Unknown: return "unknown menu framework provider";
        }
        return "unknown menu framework provider";
    }

    [[nodiscard]] constexpr std::string_view MenuFrameworkCompatibilityLabel(
        MenuFrameworkCompatibility compatibility) noexcept
    {
        switch (compatibility) {
        case MenuFrameworkCompatibility::Compatible: return "compatible";
        case MenuFrameworkCompatibility::ModuleUnavailable: return "module unavailable";
        case MenuFrameworkCompatibility::UnknownProvider: return "unknown provider";
        case MenuFrameworkCompatibility::VersionUnavailable: return "fixed version unavailable";
        case MenuFrameworkCompatibility::UnsupportedVersion: return "unsupported fixed version";
        case MenuFrameworkCompatibility::MissingRequiredExport: return "required export missing";
        }
        return "unknown";
    }
}
