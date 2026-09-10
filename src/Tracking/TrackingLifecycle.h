#pragma once

#include <cstddef>

namespace whereabouts
{
    struct TrackingLifecycleState
    {
        bool questRunning{false};
        bool scriptBound{false};
        std::size_t activeAliases{0};
        bool questStarting{false};
        bool uninstallLocked{false};
        bool questStopped{false};
    };

    enum class TrackingLifecycleEvent
    {
        TrackRequested,
        ReadinessTick,
        PostLoadRepair,
        FinalAliasCleared,
        TrackedDeathRemoved,
        PrepareForUninstall
    };

    enum class TrackingLifecycleAction
    {
        None,
        StartQuest,
        WaitForBinding,
        DispatchTrack,
        DispatchRepair,
        StopAndReset
    };

    [[nodiscard]] constexpr TrackingLifecycleAction DecideTrackingLifecycle(
        const TrackingLifecycleState& state,
        TrackingLifecycleEvent event) noexcept
    {
        if (state.uninstallLocked) return TrackingLifecycleAction::None;
        if (event == TrackingLifecycleEvent::FinalAliasCleared ||
            event == TrackingLifecycleEvent::TrackedDeathRemoved) {
            return TrackingLifecycleAction::None;
        }
        if (event == TrackingLifecycleEvent::PrepareForUninstall) {
            return state.activeAliases == 0 && state.questRunning ?
                TrackingLifecycleAction::StopAndReset : TrackingLifecycleAction::None;
        }
        if (event == TrackingLifecycleEvent::PostLoadRepair && state.activeAliases == 0) {
            return TrackingLifecycleAction::None;
        }
        if (state.questStarting) return TrackingLifecycleAction::WaitForBinding;
        if (state.questStopped || !state.questRunning) return TrackingLifecycleAction::StartQuest;
        if (!state.scriptBound) return TrackingLifecycleAction::WaitForBinding;
        return event == TrackingLifecycleEvent::PostLoadRepair ?
            TrackingLifecycleAction::DispatchRepair : TrackingLifecycleAction::DispatchTrack;
    }
}
