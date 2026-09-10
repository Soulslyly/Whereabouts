#pragma once

#include "Core/TrackingCompletion.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace whereabouts
{
    struct EnabledStateCompletion
    {
        std::uint32_t runtimeFormID{0};
        bool expectedEnabled{false};
        OperationEpochToken epoch;
        std::uint64_t requestSerial{0};
    };

    struct UiCompletionBatch
    {
        std::vector<TrackingCompletion> tracking;
        std::vector<EnabledStateCompletion> enabled;
    };

    class UiCompletionMailbox
    {
    public:
        void PushTracking(TrackingCompletion completion);
        void PushEnabled(EnabledStateCompletion completion);
        [[nodiscard]] UiCompletionBatch Drain();
        void Clear() noexcept;

    private:
        std::mutex mutex_;
        UiCompletionBatch pending_;
    };
}
