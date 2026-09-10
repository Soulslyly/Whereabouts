#include "Tracking/TrackingOperationLedger.h"

#include <algorithm>
#include <limits>

namespace whereabouts
{
    std::optional<TrackingOperationTicket> TrackingOperationLedger::TryBegin(
        OperationEpochToken epoch,
        TrackingOperation operation,
        std::optional<std::uint32_t> referenceHandle,
        std::optional<std::uint16_t> objectiveIndex)
    {
        if (!epoch) return std::nullopt;

        std::scoped_lock lock(mutex_);
        if (active_) return std::nullopt;

        active_ = TrackingOperationTicket{
            epoch, NextSerial(), operation, referenceHandle, objectiveIndex};
        return active_;
    }

    bool TrackingOperationLedger::Complete(const TrackingOperationTicket& ticket)
    {
        std::scoped_lock lock(mutex_);
        if (!active_ || *active_ != ticket) return false;
        active_.reset();
        return true;
    }

    bool TrackingOperationLedger::Busy() const
    {
        std::scoped_lock lock(mutex_);
        return active_.has_value();
    }

    void TrackingOperationLedger::Invalidate()
    {
        std::scoped_lock lock(mutex_);
        static_cast<void>(NextSerial());
        active_.reset();
        repairQueued_ = false;
        retirements_.clear();
    }

    void TrackingOperationLedger::QueueRepair()
    {
        std::scoped_lock lock(mutex_);
        repairQueued_ = true;
    }

    bool TrackingOperationLedger::TakeRepair()
    {
        std::scoped_lock lock(mutex_);
        const auto queued = repairQueued_;
        repairQueued_ = false;
        return queued;
    }

    void TrackingOperationLedger::QueueRetirement(
        std::uint16_t objectiveIndex,
        std::uint32_t referenceHandle,
        std::uint32_t runtimeFormID)
    {
        std::scoped_lock lock(mutex_);
        const PendingRetirement item{objectiveIndex, referenceHandle, runtimeFormID};
        if (std::ranges::find(retirements_, item) == retirements_.end()) {
            retirements_.push_back(item);
        }
    }

    std::vector<PendingRetirement> TrackingOperationLedger::TakeRetirements()
    {
        std::scoped_lock lock(mutex_);
        auto result = std::move(retirements_);
        retirements_.clear();
        return result;
    }

    std::uint64_t TrackingOperationLedger::NextSerial() noexcept
    {
        serial_ = serial_ == std::numeric_limits<std::uint64_t>::max() ? 1 : serial_ + 1;
        return serial_;
    }
}
