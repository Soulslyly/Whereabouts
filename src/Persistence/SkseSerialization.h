#pragma once

#include "Lifecycle/OperationEpoch.h"

namespace whereabouts
{
    using SessionBoundaryBeginCallback = OperationEpochToken(*)() noexcept;
    using SessionBoundaryEndCallback = void(*)(OperationEpochToken) noexcept;

    inline OperationEpochToken BeginSerializationBoundary(
        SessionBoundaryBeginCallback callback) noexcept
    {
        return callback ? callback() : OperationEpochToken{};
    }

    inline void EndSerializationBoundary(
        SessionBoundaryEndCallback callback,
        OperationEpochToken token) noexcept
    {
        if (callback && token) callback(token);
    }

    [[nodiscard]] bool RegisterSkseSerialization();
}
