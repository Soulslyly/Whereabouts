#include "API/PublicApi.h"

#include "Core/SpatialPresentation.h"

#include <algorithm>
#include <format>

namespace whereabouts
{
    std::string FormatPublicRuntimeFormID(std::uint32_t formID)
    {
        return formID == 0 ? std::string{} : std::format("{:08X}", formID);
    }

    std::string FormatPublicStableID(const FormIdentity& identity)
    {
        return identity.IsPersistable() ?
            std::format("{}:{:06X}", identity.plugin, identity.localID) :
            std::string{};
    }

    namespace
    {
        PublicNpcSex ToPublicNpcSex(NpcSex sex) noexcept
        {
            switch (sex) {
            case NpcSex::Male: return PublicNpcSex::Male;
            case NpcSex::Female: return PublicNpcSex::Female;
            case NpcSex::Unknown: return PublicNpcSex::Unknown;
            }
            return PublicNpcSex::Unknown;
        }

        PublicAreaType ToPublicAreaType(SpatialKind kind) noexcept
        {
            switch (kind) {
            case SpatialKind::Interior: return PublicAreaType::Interior;
            case SpatialKind::Exterior: return PublicAreaType::Exterior;
            case SpatialKind::Unknown: return PublicAreaType::Unknown;
            }
            return PublicAreaType::Unknown;
        }

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

        auto stableReferenceID = FormatPublicStableID(found->StableReference());
        const bool favorite = found->StableReference().IsPersistable() &&
            std::ranges::any_of(favoriteIdentities, [&](const auto& identity) {
                return identity == found->StableReference();
            });
        const auto& projection = found->recordProjection;
        const bool traitsKnown = projection && projection->traitsKnown;
        const bool actorFlagsKnown = found->actorFlagsKnown;
        const bool factionsKnown = projection && projection->factionsKnown;
        const bool keywordsKnown = projection && projection->keywordsKnown;
        return PublicNpcRecord{
            .name = found->displayName,
            .stableReferenceID = std::move(stableReferenceID),
            .referenceEditorID = found->referenceEditorID,
            .baseEditorID = found->baseEditorID,
            .location = found->spatial.location,
            .cell = found->spatial.cell,
            .worldspace = found->spatial.worldspace,
            .worldspaceFormID = FormatPublicRuntimeFormID(found->spatial.worldspaceFormID),
            .locationStatus = ToPublicLocationStatus(found->spatial.freshness),
            .areaType = ToPublicAreaType(found->spatial.kind),
            .traitsKnown = traitsKnown,
            .race = traitsKnown ? projection->race : std::string{},
            .sex = traitsKnown ? ToPublicNpcSex(projection->sex) : PublicNpcSex::Unknown,
            .actorFlagsKnown = actorFlagsKnown,
            .essential = actorFlagsKnown && found->essential,
            .protectedActor = actorFlagsKnown && found->protectedActor,
            .factionsKnown = factionsKnown,
            .baseKeywordsKnown = keywordsKnown,
            .recordProjection = projection,
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
