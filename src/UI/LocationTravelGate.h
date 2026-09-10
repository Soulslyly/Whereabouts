#pragma once

#include <cstdint>
#include <limits>

namespace whereabouts::ui
{
    class LocationTravelGate
    {
    public:
        [[nodiscard]] std::uint64_t Arm() noexcept
        {
            if (generation_ == (std::numeric_limits<std::uint64_t>::max)()) generation_ = 1;
            else if (++generation_ == 0) generation_ = 1;
            pending_ = true;
            return generation_;
        }

        [[nodiscard]] bool TakeForDispatch(
            std::uint64_t generation,
            bool frameworkWindowOpen) noexcept
        {
            if (!pending_ || frameworkWindowOpen || generation != generation_) return false;
            pending_ = false;
            return true;
        }

        void Cancel() noexcept
        {
            pending_ = false;
            if (generation_ == (std::numeric_limits<std::uint64_t>::max)()) generation_ = 1;
            else if (++generation_ == 0) generation_ = 1;
        }

        [[nodiscard]] bool Pending() const noexcept { return pending_; }
        [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

    private:
        std::uint64_t generation_{0};
        bool pending_{false};
    };
}
