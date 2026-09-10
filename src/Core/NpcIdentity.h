#pragma once

#include "Core/FormIdentity.h"

#include <cstdint>
#include <string_view>

namespace whereabouts
{
    struct RuntimeFormIdentity
    {
        std::uint32_t runtimeFormID{0};
        FormIdentity stable;

        [[nodiscard]] bool operator==(const RuntimeFormIdentity&) const noexcept = default;
    };

    struct NpcIdentity
    {
        RuntimeFormIdentity reference;
        RuntimeFormIdentity base;
        bool uniqueBase{true};

        [[nodiscard]] bool operator==(const NpcIdentity&) const noexcept = default;

        [[nodiscard]] std::uint32_t ReferenceRuntimeID() const noexcept
        {
            return reference.runtimeFormID;
        }

        [[nodiscard]] std::uint32_t BaseRuntimeID() const noexcept
        {
            return base.runtimeFormID;
        }

        [[nodiscard]] const FormIdentity& StableReference() const noexcept
        {
            return reference.stable;
        }

        [[nodiscard]] const FormIdentity& StableBase() const noexcept
        {
            return base.stable;
        }

        [[nodiscard]] bool IsPersistable() const noexcept
        {
            return reference.stable.IsPersistable();
        }

        [[nodiscard]] bool IsUniqueBase() const noexcept
        {
            return uniqueBase;
        }

        [[nodiscard]] std::string_view SourcePlugin() const noexcept
        {
            if (reference.stable.IsPersistable()) return reference.stable.plugin;
            return base.stable.plugin;
        }

        [[nodiscard]] bool MatchesStableReference(const FormIdentity& value) const noexcept
        {
            return reference.stable.IsPersistable() && reference.stable == value;
        }

        [[nodiscard]] bool SameReferenceInSession(const NpcIdentity& other) const noexcept
        {
            if (reference.stable.IsPersistable() && other.reference.stable.IsPersistable()) {
                return reference.stable == other.reference.stable;
            }
            return reference.runtimeFormID != 0 &&
                   reference.runtimeFormID == other.reference.runtimeFormID;
        }
    };

    [[nodiscard]] inline bool ResolvedReferenceMatches(
        const NpcIdentity& expected,
        const NpcIdentity& resolved) noexcept
    {
        if (expected.reference.stable.IsPersistable()) {
            return resolved.reference.stable.IsPersistable() &&
                expected.reference.stable == resolved.reference.stable;
        }
        return expected.ReferenceRuntimeID() != 0 &&
            expected.ReferenceRuntimeID() == resolved.ReferenceRuntimeID();
    }
}
