#include "PCH.h"

#include "Search/IndexCoordinator.h"
#include "Search/RuntimeIndex.h"
#include "Tracking/TrackingService.h"

namespace whereabouts
{
    namespace
    {
        IndexCoordinatorServices BindSkyrimServices(TrackingService& tracking)
        {
            IndexCoordinatorServices services;
            services.queueTask = [](std::function<void()> task) {
                const auto* tasks = SKSE::GetTaskInterface();
                if (!tasks) return false;
                tasks->AddTask([task = std::move(task)]() mutable { task(); });
                return true;
            };
            services.captureTrackedRuntimeIds = [&tracking] {
                const auto handles = tracking.ActiveAliases();
                std::vector<std::uint32_t> trackedIds;
                trackedIds.reserve(handles.size());
                for (const auto& handle : handles) {
                    if (const auto reference = handle.get()) {
                        trackedIds.push_back(reference->GetFormID());
                    }
                }
                return trackedIds;
            };
            services.repairMarkers = [&tracking] { return tracking.RefreshMarkers(); };
            return services;
        }
    }

    IndexCoordinator::IndexCoordinator(
        RuntimeIndex& index,
        TrackingService& tracking) noexcept :
        IndexCoordinator(index, BindSkyrimServices(tracking))
    {}
}
