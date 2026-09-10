#pragma once

#include "Core/RuntimeIndexSnapshot.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace whereabouts
{
    inline constexpr std::int32_t kPublicApiVersion = 1;

    enum class PublicLocationStatus : std::int32_t
    {
        Unavailable = 0,
        Current = 1,
        LastObserved = 2
    };

    struct PublicNpcRecord
    {
        std::string name;
        std::string stableReferenceID;
        std::string referenceEditorID;
        std::string baseEditorID;
        std::string location;
        std::string cell;
        std::string worldspace;
        PublicLocationStatus locationStatus{PublicLocationStatus::Unavailable};
        bool alive{false};
        bool enabled{false};
        bool loaded{false};
        bool follower{false};
        bool potentialFollower{false};
        bool tracked{false};
        bool favorite{false};
        bool generic{false};

        [[nodiscard]] bool operator==(const PublicNpcRecord&) const noexcept = default;
    };

    [[nodiscard]] bool IsPublicApiReady(
        const RuntimeIndexView& view,
        std::uint64_t currentSession,
        bool runtimeReady) noexcept;

    [[nodiscard]] std::optional<PublicNpcRecord> QueryPublicNpc(
        const RuntimeIndexView& view,
        std::uint64_t currentSession,
        bool runtimeReady,
        std::uint32_t referenceRuntimeFormID,
        std::span<const FormIdentity> favoriteIdentities = {});
}
