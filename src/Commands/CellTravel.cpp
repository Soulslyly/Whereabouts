#include "Commands/CellTravel.h"

#include <cmath>

namespace whereabouts
{
    void ReturnPointStore::BeginSession(OperationEpochToken token) noexcept
    {
        try {
            std::scoped_lock lock(mutex_);
            if (session_ == token) return;
            session_ = token;
            point_.reset();
        } catch (...) {}
    }

    void ReturnPointStore::Clear() noexcept
    {
        try {
            std::scoped_lock lock(mutex_);
            session_ = {};
            point_.reset();
        } catch (...) {}
    }

    void ReturnPointStore::Replace(ReturnPoint point) noexcept
    {
        if (!point.session || !point.cell.IsPersistable() || point.cellRuntimeFormID == 0) return;
        try {
            std::scoped_lock lock(mutex_);
            if (session_ != point.session) return;
            point_ = std::move(point);
        } catch (...) {}
    }

    std::optional<ReturnPoint> ReturnPointStore::Current(
        OperationEpochToken token) const noexcept
    {
        if (!token) return std::nullopt;
        try {
            std::scoped_lock lock(mutex_);
            if (session_ != token || !point_ || point_->session != token) return std::nullopt;
            return point_;
        } catch (...) {
            return std::nullopt;
        }
    }

    bool ReturnPointStore::ConsumeIfMatches(
        const ReturnPoint& point,
        OperationEpochToken token) noexcept
    {
        if (!token) return false;
        try {
            std::scoped_lock lock(mutex_);
            if (session_ != token || !point_ || point_->session != token || *point_ != point) {
                return false;
            }
            point_.reset();
            return true;
        } catch (...) {
            return false;
        }
    }

    bool ReturnTransformMatches(
        const TransformPoint& expectedPosition,
        const TransformPoint& expectedAngle,
        const TransformPoint& actualPosition,
        const TransformPoint& actualAngle) noexcept
    {
        const auto close = [](float left, float right, float tolerance) {
            return std::abs(left - right) <= tolerance;
        };
        return close(expectedPosition.x, actualPosition.x, 2.0F) &&
            close(expectedPosition.y, actualPosition.y, 2.0F) &&
            close(expectedPosition.z, actualPosition.z, 2.0F) &&
            close(expectedAngle.x, actualAngle.x, 0.01F) &&
            close(expectedAngle.y, actualAngle.y, 0.01F) &&
            close(expectedAngle.z, actualAngle.z, 0.01F);
    }
}
