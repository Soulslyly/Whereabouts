#pragma once

#include "Core/NpcIdentity.h"
#include "Core/NpcSex.h"
#include "Core/RecordProjection.h"
#include "Core/SpatialSnapshot.h"

#include <cstdint>
#include <optional>
#include <memory>
#include <string>

namespace whereabouts
{
    struct NpcSearchKeys
    {
        std::string name;
        std::string editorIDs;
        std::string plugin;
        std::string location;
        std::string race;

        [[nodiscard]] bool operator==(const NpcSearchKeys&) const noexcept = default;
    };

    struct NpcSnapshot
    {
        NpcIdentity identity;
        NpcSearchKeys searchKeys;
        std::string displayName;
        std::string referenceEditorID;
        std::string baseEditorID;
        std::shared_ptr<const NpcRecordProjection> recordProjection;
        SpatialSnapshot spatial;
        std::string race;
        NpcSex sex{NpcSex::Unknown};
        std::uint16_t level{0};
        float health{0.0F};
        float magicka{0.0F};
        float stamina{0.0F};
        bool alive{false};
        bool enabled{false};
        bool teammate{false};
        bool potentialFollower{false};
        bool loaded{false};
        bool tracked{false};
        bool favorite{false};
        bool available{false};
        bool actorFlagsKnown{false};
        bool essential{false};
        bool protectedActor{false};
        bool trackingFull{false};

        [[nodiscard]] bool operator==(const NpcSnapshot&) const noexcept = default;

        [[nodiscard]] std::uint32_t ReferenceRuntimeID() const noexcept
        {
            return identity.ReferenceRuntimeID();
        }

        [[nodiscard]] std::uint32_t BaseRuntimeID() const noexcept
        {
            return identity.BaseRuntimeID();
        }

        [[nodiscard]] const FormIdentity& StableReference() const noexcept
        {
            return identity.StableReference();
        }

        [[nodiscard]] const FormIdentity& StableBase() const noexcept
        {
            return identity.StableBase();
        }

        [[nodiscard]] std::string_view SourcePlugin() const noexcept
        {
            return identity.SourcePlugin();
        }

        [[nodiscard]] bool IsUniqueBase() const noexcept
        {
            return identity.IsUniqueBase();
        }
    };
}
