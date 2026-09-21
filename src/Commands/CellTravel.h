#pragma once

#include "Core/NpcSnapshot.h"
#include "Lifecycle/OperationEpoch.h"

#include <cstdint>
#include <mutex>
#include <optional>

namespace whereabouts
{
    struct TransformPoint
    {
        float x{0.0F};
        float y{0.0F};
        float z{0.0F};

        [[nodiscard]] bool operator==(const TransformPoint&) const noexcept = default;
    };

    enum class ContextualMovementAction
    {
        None,
        TravelToNpc,
        TravelToCell,
        Return
    };

    [[nodiscard]] constexpr ContextualMovementAction ClassifyContextualMovement(
        bool actorAvailable,
        const std::optional<RecordedCellSnapshot>& recordedCell,
        bool cellResolvable) noexcept
    {
        if (actorAvailable) return ContextualMovementAction::TravelToNpc;
        if (recordedCell && recordedCell->Known() && cellResolvable) {
            return ContextualMovementAction::TravelToCell;
        }
        return ContextualMovementAction::None;
    }

    struct ReturnPoint
    {
        FormIdentity cell;
        std::uint32_t cellRuntimeFormID{0};
        FormIdentity worldspace;
        TransformPoint position;
        TransformPoint angle;
        OperationEpochToken session;

        [[nodiscard]] bool operator==(const ReturnPoint& other) const noexcept
        {
            return cell == other.cell && cellRuntimeFormID == other.cellRuntimeFormID &&
                worldspace == other.worldspace && session == other.session &&
                position.x == other.position.x && position.y == other.position.y &&
                position.z == other.position.z && angle.x == other.angle.x &&
                angle.y == other.angle.y && angle.z == other.angle.z;
        }
    };

    class ReturnPointStore
    {
    public:
        void BeginSession(OperationEpochToken token) noexcept;
        void Clear() noexcept;
        void Replace(ReturnPoint point) noexcept;
        [[nodiscard]] std::optional<ReturnPoint> Current(
            OperationEpochToken token) const noexcept;
        [[nodiscard]] bool ConsumeIfMatches(
            const ReturnPoint& point,
            OperationEpochToken token) noexcept;

    private:
        mutable std::mutex mutex_;
        OperationEpochToken session_;
        std::optional<ReturnPoint> point_;
    };

    [[nodiscard]] bool ReturnTransformMatches(
        const TransformPoint& expectedPosition,
        const TransformPoint& expectedAngle,
        const TransformPoint& actualPosition,
        const TransformPoint& actualAngle) noexcept;
}
