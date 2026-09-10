#include "Lifecycle/OperationQueue.h"

#include <utility>

namespace whereabouts
{
    OperationQueue::OperationQueue(
        const OperationEpoch& epoch,
        Submitter gameSubmitter,
        Submitter uiSubmitter,
        ExceptionSink exceptionSink) noexcept :
        epoch_(epoch),
        gameSubmitter_(std::move(gameSubmitter)),
        uiSubmitter_(std::move(uiSubmitter)),
        exceptionSink_(std::move(exceptionSink))
    {}

    std::expected<void, OperationQueueError> OperationQueue::SubmitGame(
        OperationEpochToken token,
        Task task) const noexcept
    {
        return Submit(gameSubmitter_, token, std::move(task));
    }

    std::expected<void, OperationQueueError> OperationQueue::SubmitUi(
        OperationEpochToken token,
        Task task) const noexcept
    {
        return Submit(uiSubmitter_, token, std::move(task));
    }

    std::expected<void, OperationQueueError> OperationQueue::Submit(
        const Submitter& submitter,
        OperationEpochToken token,
        Task task) const noexcept
    {
        if (!epoch_.IsCurrent(token)) return std::unexpected(OperationQueueError::StaleEpoch);
        if (!submitter) return std::unexpected(OperationQueueError::TaskInterfaceUnavailable);
        try {
            submitter([this, token, task = std::move(task)]() mutable noexcept {
                if (!epoch_.IsCurrent(token)) return;
                try {
                    if (task) task();
                } catch (...) {
                    ReportContainedException();
                }
            });
            return {};
        } catch (...) {
            return std::unexpected(OperationQueueError::SubmissionException);
        }
    }

    void OperationQueue::ReportContainedException() const noexcept
    {
        try {
            if (exceptionSink_) exceptionSink_();
        } catch (...) {
        }
    }
}
