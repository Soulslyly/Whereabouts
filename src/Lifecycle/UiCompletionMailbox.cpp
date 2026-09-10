#include "Lifecycle/UiCompletionMailbox.h"

#include <utility>

namespace whereabouts
{
    void UiCompletionMailbox::PushTracking(TrackingCompletion completion)
    {
        std::scoped_lock lock(mutex_);
        pending_.tracking.push_back(std::move(completion));
    }

    void UiCompletionMailbox::PushEnabled(EnabledStateCompletion completion)
    {
        std::scoped_lock lock(mutex_);
        pending_.enabled.push_back(completion);
    }

    UiCompletionBatch UiCompletionMailbox::Drain()
    {
        UiCompletionBatch result;
        std::scoped_lock lock(mutex_);
        std::swap(result, pending_);
        return result;
    }

    void UiCompletionMailbox::Clear() noexcept
    {
        try {
            std::scoped_lock lock(mutex_);
            pending_ = {};
        } catch (...) {
        }
    }
}
