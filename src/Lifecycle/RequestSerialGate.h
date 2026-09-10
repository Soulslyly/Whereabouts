#pragma once

#include <atomic>
#include <cstdint>

namespace whereabouts
{
    class RequestSerialGate
    {
    public:
        struct TestState { std::uint64_t serial; };

        RequestSerialGate() noexcept = default;
        explicit RequestSerialGate(TestState state) noexcept : serial_(state.serial) {}

        [[nodiscard]] std::uint64_t Issue() noexcept
        {
            auto current = serial_.load(std::memory_order_relaxed);
            for (;;) {
                const auto next = current == UINT64_MAX ? 1U : current + 1U;
                if (serial_.compare_exchange_weak(
                        current, next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    return next;
                }
            }
        }

        [[nodiscard]] bool IsCurrent(std::uint64_t serial) const noexcept
        {
            return serial != 0 && serial_.load(std::memory_order_acquire) == serial;
        }

        void Invalidate() noexcept { static_cast<void>(Issue()); }

    private:
        std::atomic_uint64_t serial_{0};
    };
}
