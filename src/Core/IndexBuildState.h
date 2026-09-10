#pragma once

#include "Core/RuntimeIndexSnapshot.h"

namespace whereabouts
{
    enum class IndexRequestReason
    {
        FirstOpen,
        TargetRefresh,
        NewGame,
        PostLoad,
        Manual
    };

    enum class IndexRequestAction
    {
        Queue,
        Coalesce,
        AlreadyReady
    };

    [[nodiscard]] constexpr bool AllowsMarkerRepair(IndexRequestReason reason) noexcept
    {
        return reason == IndexRequestReason::NewGame ||
               reason == IndexRequestReason::PostLoad ||
               reason == IndexRequestReason::Manual;
    }

    [[nodiscard]] constexpr bool MergeMarkerRepairIntent(
        bool current,
        IndexRequestReason incoming) noexcept
    {
        return current || AllowsMarkerRepair(incoming);
    }

    [[nodiscard]] constexpr IndexRequestAction DecideIndexRequest(
        IndexReadiness readiness,
        bool force) noexcept
    {
        if (readiness == IndexReadiness::Queued || readiness == IndexReadiness::Building) {
            return IndexRequestAction::Coalesce;
        }
        if (readiness == IndexReadiness::Ready && !force) {
            return IndexRequestAction::AlreadyReady;
        }
        return IndexRequestAction::Queue;
    }
}
