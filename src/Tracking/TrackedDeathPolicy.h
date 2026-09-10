#pragma once

#include "Persistence/Serialization.h"

namespace whereabouts
{
    [[nodiscard]] inline bool TrackedDeathReferenceMatches(
        std::uint32_t storedRuntimeFormID,
        const std::optional<FormIdentity>& storedIdentity,
        std::uint32_t currentRuntimeFormID,
        const std::optional<FormIdentity>& currentIdentity) noexcept
    {
        if (storedIdentity) return currentIdentity && *storedIdentity == *currentIdentity;
        return storedRuntimeFormID == currentRuntimeFormID;
    }

    struct TrackedDeathRuntimeState
    {
        TrackedDeathState state{TrackedDeathState::BodyPresent};
        bool aliasOccupied{false};
        bool aliasMatches{false};
        bool actorAlive{false};
        bool actorDisabled{false};
    };

    enum class TrackedDeathAction
    {
        None,
        KeepMarker,
        ForgetDeath,
        RetireMarker,
        RetireEmptyMarker,
        MarkMissingWithoutClearing
    };

    [[nodiscard]] constexpr TrackedDeathAction DecideTrackedDeathAction(
        const TrackedDeathRuntimeState& state) noexcept
    {
        if (state.state == TrackedDeathState::BodyMissing) {
            return TrackedDeathAction::None;
        }
        if (!state.aliasMatches) {
            return state.aliasOccupied ?
                TrackedDeathAction::MarkMissingWithoutClearing :
                TrackedDeathAction::RetireEmptyMarker;
        }
        if (state.actorAlive) return TrackedDeathAction::ForgetDeath;
        if (state.actorDisabled) return TrackedDeathAction::RetireMarker;
        return TrackedDeathAction::KeepMarker;
    }
}
