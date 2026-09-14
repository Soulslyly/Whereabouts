#pragma once

#include "Core/RuntimeIndexSnapshot.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace whereabouts
{
    inline constexpr std::int32_t kPublicApiVersion = 2;

    [[nodiscard]] constexpr bool SupportsPublicApiVersion(std::int32_t version) noexcept
    {
        return version >= 1 && version <= kPublicApiVersion;
    }

    enum class PublicLocationStatus : std::int32_t
    {
        Unavailable = 0,
        Current = 1,
        LastObserved = 2
    };

    enum class PublicNpcSex : std::int32_t
    {
        Unknown = 0,
        Male = 1,
        Female = 2
    };

    enum class PublicAreaType : std::int32_t
    {
        Unknown = 0,
        Interior = 1,
        Exterior = 2
    };

    struct PublicNpcRecord
    {
        std::string name;
        std::string stableReferenceID;
        std::string referenceEditorID;
        std::string baseEditorID;
        std::string location;
        std::string cell;
        std::string worldspace;
        std::string worldspaceFormID;
        PublicLocationStatus locationStatus{PublicLocationStatus::Unavailable};
        PublicAreaType areaType{PublicAreaType::Unknown};
        bool traitsKnown{false};
        std::string race;
        PublicNpcSex sex{PublicNpcSex::Unknown};
        bool actorFlagsKnown{false};
        bool essential{false};
        bool protectedActor{false};
        bool factionsKnown{false};
        bool baseKeywordsKnown{false};
        std::shared_ptr<const NpcRecordProjection> recordProjection;
        bool alive{false};
        bool enabled{false};
        bool loaded{false};
        bool follower{false};
        bool potentialFollower{false};
        bool tracked{false};
        bool favorite{false};
        bool generic{false};

        [[nodiscard]] bool operator==(const PublicNpcRecord&) const noexcept = default;
    };

    [[nodiscard]] std::string FormatPublicRuntimeFormID(std::uint32_t formID);
    [[nodiscard]] std::string FormatPublicStableID(const FormIdentity& identity);

    [[nodiscard]] bool IsPublicApiReady(
        const RuntimeIndexView& view,
        std::uint64_t currentSession,
        bool runtimeReady) noexcept;

    [[nodiscard]] std::optional<PublicNpcRecord> QueryPublicNpc(
        const RuntimeIndexView& view,
        std::uint64_t currentSession,
        bool runtimeReady,
        std::uint32_t referenceRuntimeFormID,
        std::span<const FormIdentity> favoriteIdentities = {});
}
