#pragma once

#include "Lifecycle/OperationEpoch.h"

#include <expected>
#include <functional>

namespace whereabouts
{
    enum class OperationQueueError
    {
        StaleEpoch,
        TaskInterfaceUnavailable,
        SubmissionException
    };

    class OperationQueue
    {
    public:
        using Task = std::function<void()>;
        using Submitter = std::function<void(Task)>;
        using ExceptionSink = std::function<void()>;

        OperationQueue(
            const OperationEpoch& epoch,
            Submitter gameSubmitter,
            Submitter uiSubmitter,
            ExceptionSink exceptionSink = {}) noexcept;

        [[nodiscard]] std::expected<void, OperationQueueError> SubmitGame(
            OperationEpochToken token,
            Task task) const noexcept;
        [[nodiscard]] std::expected<void, OperationQueueError> SubmitUi(
            OperationEpochToken token,
            Task task) const noexcept;

    private:
        [[nodiscard]] std::expected<void, OperationQueueError> Submit(
            const Submitter& submitter,
            OperationEpochToken token,
            Task task) const noexcept;
        void ReportContainedException() const noexcept;

        const OperationEpoch& epoch_;
        Submitter gameSubmitter_;
        Submitter uiSubmitter_;
        ExceptionSink exceptionSink_;
    };
}
