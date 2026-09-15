#pragma once

#include <string_view>

namespace whereabouts
{
    enum class VrEnvironmentCompatibility
    {
        Compatible,
        DataHandlerUnavailable,
        PluginUnavailable
    };

    struct VrEnvironmentProbe
    {
        bool dataHandlerAvailable{false};
        bool lightPluginLoaded{false};
    };

    [[nodiscard]] constexpr VrEnvironmentCompatibility DecideVrEnvironmentCompatibility(
        bool isVr,
        bool dataHandlerAvailable,
        bool lightPluginLoaded) noexcept
    {
        if (!isVr) return VrEnvironmentCompatibility::Compatible;
        if (!dataHandlerAvailable) return VrEnvironmentCompatibility::DataHandlerUnavailable;
        if (!lightPluginLoaded) return VrEnvironmentCompatibility::PluginUnavailable;
        return VrEnvironmentCompatibility::Compatible;
    }

    [[nodiscard]] constexpr std::string_view VrEnvironmentCompatibilityLabel(
        VrEnvironmentCompatibility compatibility) noexcept
    {
        switch (compatibility) {
        case VrEnvironmentCompatibility::Compatible: return "compatible";
        case VrEnvironmentCompatibility::DataHandlerUnavailable: return "VR-aware data handler unavailable";
        case VrEnvironmentCompatibility::PluginUnavailable: return "Whereabouts.esp light plugin unavailable";
        }
        return "unknown";
    }

    [[nodiscard]] VrEnvironmentProbe ProbeLoadedVrEnvironment() noexcept;
}
