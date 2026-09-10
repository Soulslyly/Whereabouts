#include "Lifecycle/OperationEpoch.h"

namespace whereabouts
{
    OperationEpoch::OperationEpoch() noexcept = default;

    OperationEpoch::OperationEpoch(TestState state) noexcept :
        state_((state.accepting ? kAccepting : 0U) |
               ((state.epoch & kEpochMask) == 0 ? 1U : (state.epoch & kEpochMask)))
    {}

    std::uint32_t OperationEpoch::NextEpoch(std::uint32_t state) noexcept
    {
        const auto current = state & kEpochMask;
        return current == kEpochMask ? 1U : current + 1U;
    }

    OperationEpochToken OperationEpoch::Token(std::uint32_t state) noexcept
    {
        return {static_cast<std::int32_t>(state & kEpochMask)};
    }

    std::optional<OperationEpochToken> OperationEpoch::ActivateInitial() noexcept
    {
        std::scoped_lock lock(transitionMutex_);
        const auto state = state_.load(std::memory_order_acquire);
        if ((state & kAccepting) != 0) return Token(state);
        if ((state & kEpochMask) != 1U) return std::nullopt;
        state_.store(state | kAccepting, std::memory_order_release);
        return Token(state);
    }

    std::optional<OperationEpochToken> OperationEpoch::Capture() const noexcept
    {
        const auto state = state_.load(std::memory_order_acquire);
        return (state & kAccepting) != 0 ? std::optional{Token(state)} : std::nullopt;
    }

    bool OperationEpoch::IsCurrent(OperationEpochToken token) const noexcept
    {
        if (!token) return false;
        const auto state = state_.load(std::memory_order_acquire);
        return (state & kAccepting) != 0 && Token(state) == token;
    }

    OperationEpochToken OperationEpoch::SuspendAndAdvance() noexcept
    {
        std::scoped_lock lock(transitionMutex_);
        auto current = state_.load(std::memory_order_relaxed);
        for (;;) {
            const auto next = NextEpoch(current);
            if (state_.compare_exchange_weak(
                    current, next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return Token(next);
            }
        }
    }

    bool OperationEpoch::Resume(OperationEpochToken expected) noexcept
    {
        if (!expected) return false;
        std::scoped_lock lock(transitionMutex_);
        auto state = static_cast<std::uint32_t>(expected.value);
        return state_.compare_exchange_strong(
            state, state | kAccepting, std::memory_order_acq_rel, std::memory_order_relaxed);
    }

    std::optional<OperationEpochToken> OperationEpoch::AdvanceActive() noexcept
    {
        std::scoped_lock lock(transitionMutex_);
        auto current = state_.load(std::memory_order_acquire);
        while ((current & kAccepting) != 0) {
            const auto next = kAccepting | NextEpoch(current);
            if (state_.compare_exchange_weak(
                    current, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return Token(next);
            }
        }
        return std::nullopt;
    }

    bool OperationEpoch::IsAccepting() const noexcept
    {
        return (state_.load(std::memory_order_acquire) & kAccepting) != 0;
    }
}
