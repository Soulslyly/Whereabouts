#pragma once

#include "Lifecycle/OperationEpoch.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace whereabouts
{
    enum class TrackingOperation
    {
        Track,
        Untrack,
        ClearAll,
        Repair,
        DeathObserved,
        DeathRemoval,
        PrepareForUninstall,
        Resume
    };

    struct TrackingCompletion
    {
        std::uint32_t runtimeFormID{0};
        TrackingOperation operation{TrackingOperation::Track};
        bool succeeded{false};
        std::string message;
        OperationEpochToken epoch;
        std::uint64_t requestSerial{0};
        std::optional<std::uint16_t> objectiveIndex;
        std::optional<std::uint32_t> referenceHandle;
        std::vector<std::uint32_t> trackedRuntimeFormIDs;
    };
}
