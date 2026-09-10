#pragma once

#include "Core/TextFold.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace whereabouts
{
    struct FormIdentity
    {
        std::string plugin;
        std::uint32_t localID{0};
        bool light{false};

        [[nodiscard]] static FormIdentity FromRuntimeFormID(
            std::string pluginName,
            std::uint32_t runtimeFormID,
            bool isLight)
        {
            constexpr std::uint32_t fullPluginMask = 0x00FFFFFF;
            constexpr std::uint32_t lightPluginMask = 0x00000FFF;

            return {
                std::move(pluginName),
                runtimeFormID & (isLight ? lightPluginMask : fullPluginMask),
                isLight};
        }

        [[nodiscard]] bool IsPersistable() const noexcept
        {
            return !plugin.empty() && localID != 0;
        }

        [[nodiscard]] bool operator==(const FormIdentity& other) const noexcept
        {
            return localID == other.localID && light == other.light &&
                   SearchTextEqualsNoexcept(plugin, other.plugin);
        }
    };
}
