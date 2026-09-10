#pragma once

#include "Core/IndexBuildState.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace whereabouts
{
    class RuntimeIndex;
    class TrackingService;

    enum class IndexRequestResult
    {
        Queued,
        Coalesced,
        AlreadyReady,
        Failed
    };

    struct IndexCoordinatorServices
    {
        // queueTask must enqueue for later execution; it must not invoke the task inline.
        std::function<bool(std::function<void()>)> queueTask;
        std::function<std::vector<std::uint32_t>()> captureTrackedRuntimeIds;
        std::function<std::expected<void, std::string>()> repairMarkers;
        std::function<void()> beforeStateObservation;
    };

    class IndexCoordinator
    {
    public:
        IndexCoordinator(RuntimeIndex& index, TrackingService& tracking) noexcept;
        IndexCoordinator(RuntimeIndex& index, IndexCoordinatorServices services) noexcept;

        [[nodiscard]] IndexRequestResult EnsureReady();
        [[nodiscard]] IndexRequestResult ForceRefresh(IndexRequestReason reason);
        [[nodiscard]] bool RebuildNow(IndexRequestReason reason);
        void BeginSessionBoundary(bool publishEmpty = true) noexcept;

    private:
        [[nodiscard]] IndexRequestResult Request(IndexRequestReason reason, bool force);
        void ExecuteBuild(std::uint64_t session) noexcept;

        RuntimeIndex& index_;
        IndexCoordinatorServices services_;
        std::mutex requestMutex_;
        bool requestActive_{false};
        bool markerRepairRequested_{false};
        std::uint64_t requestSession_{0};
    };
}
