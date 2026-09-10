#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace whereabouts
{
    struct OperationEpochToken
    {
        std::int32_t value{0};
        [[nodiscard]] explicit operator bool() const noexcept { return value > 0; }
        friend bool operator==(OperationEpochToken, OperationEpochToken) = default;
    };

    class OperationEpoch
    {
    public:
        struct TestState
        {
            std::uint32_t epoch;
            bool accepting;
        };

        OperationEpoch() noexcept;
        explicit OperationEpoch(TestState state) noexcept;

        [[nodiscard]] std::optional<OperationEpochToken> ActivateInitial() noexcept;
        [[nodiscard]] std::optional<OperationEpochToken> Capture() const noexcept;
        [[nodiscard]] bool IsCurrent(OperationEpochToken token) const noexcept;
        [[nodiscard]] OperationEpochToken SuspendAndAdvance() noexcept;
        [[nodiscard]] bool Resume(OperationEpochToken expected) noexcept;
        [[nodiscard]] std::optional<OperationEpochToken> AdvanceActive() noexcept;
        [[nodiscard]] bool IsAccepting() const noexcept;

        template <class Function>
        bool RunIfCurrent(OperationEpochToken token, Function&& function)
        {
            std::scoped_lock lock(transitionMutex_);
            if (!IsCurrent(token)) return false;
            std::forward<Function>(function)();
            return true;
        }

    private:
        static constexpr std::uint32_t kAccepting = 0x80000000U;
        static constexpr std::uint32_t kEpochMask = 0x7FFFFFFFU;

        [[nodiscard]] static std::uint32_t NextEpoch(std::uint32_t state) noexcept;
        [[nodiscard]] static OperationEpochToken Token(std::uint32_t state) noexcept;

        mutable std::mutex transitionMutex_;
        std::atomic_uint32_t state_{1U};
    };
}
