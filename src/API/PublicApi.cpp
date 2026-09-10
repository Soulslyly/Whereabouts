#include "API/PublicApi.h"

#include "Core/SpatialPresentation.h"

#include <algorithm>
#include <format>

namespace whereabouts
{
    namespace
    {
        constexpr PublicLocationStatus ToPublicLocationStatus(SpatialFreshness freshness) noexcept
        {
            switch (freshness) {
            case SpatialFreshness::Current: return PublicLocationStatus::Current;
            case SpatialFreshness::LastObserved: return PublicLocationStatus::LastObserved;
            case SpatialFreshness::Unavailable: return PublicLocationStatus::Unavailable;
            }
            return PublicLocationStatus::Unavailable;
        }
    }

    bool IsPublicApiReady(
        const RuntimeIndexView& view,
        std::uint64_t currentSession,
        bool runtimeReady) noexcept
    {
        return runtimeReady &&
            IsCurrentIndexView(view, currentSession) &&
            view->readiness == IndexReadiness::Ready &&
            view->failure == IndexFailure::None;
    }

    std::optional<PublicNpcRecord> QueryPublicNpc(
        const RuntimeIndexView& view,
        std::uint64_t currentSession,
        bool runtimeReady,
        std::uint32_t referenceRuntimeFormID,
        std::span<const FormIdentity> favoriteIdentities)
    {
        if (referenceRuntimeFormID == 0 ||
            !IsPublicApiReady(view, currentSession, runtimeReady)) {
            return std::nullopt;
        }
        const auto found = std::ranges::find_if(*view->catalog, [&](const auto& npc) {
            return npc.ReferenceRuntimeID() == referenceRuntimeFormID;
        });
        if (found == view->catalog->end()) return std::nullopt;

        std::string stableReferenceID;
        if (found->StableReference().IsPersistable()) {
            stableReferenceID = std::format(
                "{}:{:06X}",
                found->StableReference().plugin,
                found->StableReference().localID);
        }
        const bool favorite = found->StableReference().IsPersistable() &&
            std::ranges::any_of(favoriteIdentities, [&](const auto& identity) {
                return identity == found->StableReference();
            });
        return PublicNpcRecord{
            .name = found->displayName,
            .stableReferenceID = std::move(stableReferenceID),
            .referenceEditorID = found->referenceEditorID,
            .baseEditorID = found->baseEditorID,
            .location = found->spatial.location,
            .cell = found->spatial.cell,
            .worldspace = found->spatial.worldspace,
            .locationStatus = ToPublicLocationStatus(found->spatial.freshness),
            .alive = found->alive,
            .enabled = found->enabled,
            .loaded = found->loaded,
            .follower = found->teammate,
            .potentialFollower = found->potentialFollower,
            .tracked = found->tracked,
            .favorite = favorite,
            .generic = !found->IsUniqueBase()};
    }
}
