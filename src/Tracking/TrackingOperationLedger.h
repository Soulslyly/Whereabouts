#pragma once

#include "Core/TrackingCompletion.h"
#include "Lifecycle/OperationEpoch.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace whereabouts
{
    struct TrackingOperationTicket
    {
        OperationEpochToken epoch;
        std::uint64_t serial{0};
        TrackingOperation operation{TrackingOperation::Track};
        std::optional<std::uint32_t> referenceHandle;
        std::optional<std::uint16_t> objectiveIndex;

        friend bool operator==(const TrackingOperationTicket&, const TrackingOperationTicket&) = default;
    };

    struct PendingRetirement
    {
        std::uint16_t objectiveIndex{0};
        std::uint32_t referenceHandle{0};
        std::uint32_t runtimeFormID{0};

        friend bool operator==(const PendingRetirement&, const PendingRetirement&) = default;
    };

    class TrackingOperationLedger
    {
    public:
        [[nodiscard]] std::optional<TrackingOperationTicket> TryBegin(
            OperationEpochToken epoch,
            TrackingOperation operation,
            std::optional<std::uint32_t> referenceHandle = std::nullopt,
            std::optional<std::uint16_t> objectiveIndex = std::nullopt);
        [[nodiscard]] bool Complete(const TrackingOperationTicket& ticket);
        [[nodiscard]] bool Busy() const;
        void Invalidate();

        void QueueRepair();
        [[nodiscard]] bool TakeRepair();
        void QueueRetirement(
            std::uint16_t objectiveIndex,
            std::uint32_t referenceHandle,
            std::uint32_t runtimeFormID);
        [[nodiscard]] std::vector<PendingRetirement> TakeRetirements();

    private:
        [[nodiscard]] std::uint64_t NextSerial() noexcept;

        mutable std::mutex mutex_;
        std::uint64_t serial_{0};
        std::optional<TrackingOperationTicket> active_;
        bool repairQueued_{false};
        std::vector<PendingRetirement> retirements_;
    };
}
