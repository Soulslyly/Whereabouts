#include "Core/SnapshotState.h"

#include <algorithm>
#include <unordered_set>

namespace whereabouts
{
    NpcSnapshot* FindSnapshotByRuntimeID(
        std::span<NpcSnapshot> snapshots,
        std::uint32_t runtimeFormID) noexcept
    {
        const auto found = std::ranges::find_if(snapshots, [runtimeFormID](const auto& snapshot) {
            return snapshot.ReferenceRuntimeID() == runtimeFormID;
        });
        return found == snapshots.end() ? nullptr : std::addressof(*found);
    }

    const NpcSnapshot* FindSnapshotByRuntimeID(
        std::span<const NpcSnapshot> snapshots,
        std::uint32_t runtimeFormID) noexcept
    {
        const auto found = std::ranges::find_if(snapshots, [runtimeFormID](const auto& snapshot) {
            return snapshot.ReferenceRuntimeID() == runtimeFormID;
        });
        return found == snapshots.end() ? nullptr : std::addressof(*found);
    }

    void ApplyTrackedState(
        std::span<NpcSnapshot> snapshots,
        std::span<const std::uint32_t> trackedRuntimeFormIDs)
    {
        const std::unordered_set<std::uint32_t> tracked{
            trackedRuntimeFormIDs.begin(), trackedRuntimeFormIDs.end()};
        const bool trackingFull = tracked.size() >= 100;
        for (auto& snapshot : snapshots) {
            snapshot.tracked = tracked.contains(snapshot.ReferenceRuntimeID());
            snapshot.trackingFull = trackingFull;
        }
    }

    void ApplyTrackingCompletionState(
        std::vector<NpcSnapshot>& snapshots,
        std::optional<NpcSnapshot>& selected,
        const TrackingCompletion& completion,
        std::span<const std::uint32_t> trackedRuntimeFormIDs)
    {
        if (!completion.succeeded) return;
        ApplyTrackedState(snapshots, trackedRuntimeFormIDs);
        if (!selected) return;
        if (const auto* refreshed = FindSnapshotByRuntimeID(
                std::span<const NpcSnapshot>{snapshots}, selected->ReferenceRuntimeID())) {
            selected = *refreshed;
        }
    }
}
