#include "PCH.h"

#include "Core/FormIdentityAdapter.h"

namespace whereabouts
{
    static_assert(sizeof(RE::FormID) == sizeof(std::uint32_t));

    std::optional<FormIdentity> TryGetFormIdentity(const RE::TESForm* form) noexcept
    {
        if (!form || form->IsDynamicForm()) return std::nullopt;

        const auto* file = form->GetFile(0);
        if (!file) return std::nullopt;

        FormIdentity identity{
            std::string(file->GetFilename()),
            form->GetLocalFormID(),
            file->IsLight()};
        return identity.IsPersistable() ? std::optional{std::move(identity)} : std::nullopt;
    }

    std::optional<NpcIdentity> TryGetNpcIdentity(const RE::Actor* actor) noexcept
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !base) return std::nullopt;
        NpcIdentity identity;
        identity.reference.runtimeFormID = actor->GetFormID();
        if (const auto stable = TryGetFormIdentity(actor)) identity.reference.stable = *stable;
        identity.base.runtimeFormID = base->GetFormID();
        if (const auto stable = TryGetFormIdentity(base)) identity.base.stable = *stable;
        identity.uniqueBase = base->IsUnique();
        return identity;
    }
}
