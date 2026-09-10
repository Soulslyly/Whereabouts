#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace whereabouts
{
    inline constexpr std::size_t kTrackingSlotCount = 100;
    inline constexpr std::uint32_t kFirstTrackingAliasID = 2;

    struct TrackingAliasDescriptor
    {
        std::uint32_t aliasID{0};
        bool referenceAlias{false};
    };

    struct TrackingQuestLayoutInput
    {
        std::vector<TrackingAliasDescriptor> aliases;
        std::vector<std::uint16_t> objectives;
    };

    struct TrackingQuestLayoutResult
    {
        std::array<std::size_t, kTrackingSlotCount> aliasVectorIndices{};
    };

    [[nodiscard]] std::expected<TrackingQuestLayoutResult, std::string>
        ValidateTrackingQuestLayout(const TrackingQuestLayoutInput& input);

    [[nodiscard]] constexpr std::string_view TrackingQuestUnavailableMessage(
        bool pluginLoaded) noexcept
    {
        return pluginLoaded ? "Whereabouts tracking quest record is unavailable" :
            "Whereabouts.esp is not active";
    }
}
