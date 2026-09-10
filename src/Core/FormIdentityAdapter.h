#pragma once

#include "Core/FormIdentity.h"
#include "Core/NpcIdentity.h"

#include <RE/Skyrim.h>

#include <optional>

namespace whereabouts
{
    [[nodiscard]] std::optional<FormIdentity> TryGetFormIdentity(
        const RE::TESForm* form) noexcept;
    [[nodiscard]] std::optional<NpcIdentity> TryGetNpcIdentity(
        const RE::Actor* actor) noexcept;
}
