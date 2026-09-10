#pragma once

#include "Core/NpcSnapshot.h"
#include "Core/TrackingCompletion.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace whereabouts
{
    enum class StateRefreshAction
    {
        Complete,
        WaitForResume,
        Retry,
        Expired
    };

    struct PendingEnabledState
    {
        std::uint32_t runtimeFormID{0};
        bool expectedEnabled{false};
    };

    [[nodiscard]] constexpr bool CanRequestEnabledStateChange(
        std::optional<PendingEnabledState> pending,
        std::uint32_t runtimeFormID) noexcept
    {
        return !pending || pending->runtimeFormID == runtimeFormID;
    }

    [[nodiscard]] constexpr bool NextRequestedEnabledState(
        bool observedEnabled,
        std::optional<bool> pendingExpected) noexcept
    {
        return !pendingExpected.value_or(observedEnabled);
    }

    [[nodiscard]] constexpr bool CanApplyEnabledStateCompletion(
        std::optional<PendingEnabledState> pending,
        std::uint32_t runtimeFormID,
        bool expectedEnabled,
        bool epochCurrent,
        bool requestCurrent) noexcept
    {
        return epochCurrent &&
            requestCurrent &&
            pending &&
            pending->runtimeFormID == runtimeFormID &&
            pending->expectedEnabled == expectedEnabled;
    }

    [[nodiscard]] constexpr StateRefreshAction DecideEnabledStateRefresh(
        bool actorAvailable,
        bool observedEnabled,
        bool expectedEnabled,
        bool menuPaused,
        std::size_t attemptsRemaining) noexcept
    {
        if (!actorAvailable) return StateRefreshAction::Expired;
        if (observedEnabled == expectedEnabled) return StateRefreshAction::Complete;
        if (menuPaused) {
            return attemptsRemaining > 0 ?
                StateRefreshAction::WaitForResume : StateRefreshAction::Expired;
        }
        return attemptsRemaining > 0 ? StateRefreshAction::Retry : StateRefreshAction::Expired;
    }

    [[nodiscard]] NpcSnapshot* FindSnapshotByRuntimeID(
        std::span<NpcSnapshot> snapshots,
        std::uint32_t runtimeFormID) noexcept;
    [[nodiscard]] const NpcSnapshot* FindSnapshotByRuntimeID(
        std::span<const NpcSnapshot> snapshots,
        std::uint32_t runtimeFormID) noexcept;
    void ApplyTrackedState(
        std::span<NpcSnapshot> snapshots,
        std::span<const std::uint32_t> trackedRuntimeFormIDs);
    void ApplyTrackingCompletionState(
        std::vector<NpcSnapshot>& snapshots,
        std::optional<NpcSnapshot>& selected,
        const TrackingCompletion& completion,
        std::span<const std::uint32_t> trackedRuntimeFormIDs);
}
