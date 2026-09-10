#include "PCH.h"

#include "Core/FormIdentityAdapter.h"
#include "Search/RuntimeIndex.h"
#include "Targets/TargetResolver.h"

namespace whereabouts
{
    TargetResolver::TargetResolver(RuntimeIndex& index, const RuntimeSettingsState& settings) noexcept :
        index_(index),
        settings_(settings)
    {}

    std::uint64_t TargetResolver::IssueSelectionRequest() noexcept
    {
        return selectionGate_.Issue();
    }

    bool TargetResolver::IsSelectionRequestCurrent(std::uint64_t serial) const noexcept
    {
        return selectionGate_.IsCurrent(serial);
    }

    bool TargetResolver::CaptureOnMenuOpen()
    {
        const auto console = ConsoleActor();
        const auto crosshair = CrosshairActor();
        bool consoleSuppressed = false;
        {
            std::scoped_lock lock(currentMutex_);
            consoleSuppressed = consoleSuppression_.ShouldSuppress(
                console ? console->GetFormID() : 0);
        }
        const auto settings = settings_.Snapshot();
        const auto source = ChooseCapturedTarget(
            settings->autoSelectConsoleTarget,
            settings->autoSelectCrosshairTarget,
            static_cast<bool>(console) && !consoleSuppressed,
            static_cast<bool>(crosshair));

        switch (source) {
        case TargetSource::Console:
            return Select(*console, source);
        case TargetSource::Crosshair:
            return Select(*crosshair, source);
        default:
            return false;
        }
    }

    bool TargetResolver::UseConsoleTarget()
    {
        const auto actor = ConsoleActor();
        {
            std::scoped_lock lock(currentMutex_);
            consoleSuppression_.Reset();
        }
        return actor && Select(*actor, TargetSource::Console);
    }

    bool TargetResolver::UseCrosshairTarget()
    {
        const auto actor = CrosshairActor();
        return actor && Select(*actor, TargetSource::Crosshair);
    }

    bool TargetResolver::Select(RE::Actor& actor, TargetSource source)
    {
        if (!IsValidActor(&actor) || source == TargetSource::None) return false;

        const auto identity = TryGetNpcIdentity(&actor);
        if (!identity) return false;
        SelectedTarget selected;
        selected.identity = *identity;
        if (const auto* name = actor.GetDisplayFullName()) selected.displayName = name;
        selected.handle = actor.GetHandle();
        selected.source = source;
        std::scoped_lock lock(currentMutex_);
        current_ = std::move(selected);
        return true;
    }

    bool TargetResolver::Select(const NpcSnapshot& snapshot, TargetSource source)
    {
        auto actor = index_.ResolveRuntime(snapshot.ReferenceRuntimeID());
        if (actor) {
            const auto resolved = TryGetNpcIdentity(actor.get());
            if (!resolved || !ResolvedReferenceMatches(snapshot.identity, *resolved)) actor.reset();
        }
        if (!actor && snapshot.StableReference().IsPersistable()) {
            actor = index_.Resolve(snapshot.StableReference());
        }
        if (actor) {
            const auto resolved = TryGetNpcIdentity(actor.get());
            if (!resolved || !ResolvedReferenceMatches(snapshot.identity, *resolved)) actor.reset();
        }
        return actor && Select(*actor, source);
    }

    void TargetResolver::Clear() noexcept
    {
        selectionGate_.Invalidate();
        try {
            std::scoped_lock lock(currentMutex_);
            current_.reset();
        } catch (...) {
        }
    }

    void TargetResolver::ClearAndSuppressConsoleTarget() noexcept
    {
        selectionGate_.Invalidate();
        try {
            std::scoped_lock lock(currentMutex_);
            const auto consoleRuntimeFormID = current_ && current_->source == TargetSource::Console ?
                current_->ReferenceRuntimeID() : 0;
            current_.reset();
            consoleSuppression_.Suppress(consoleRuntimeFormID);
        } catch (...) {
        }
    }

    RE::NiPointer<RE::Actor> TargetResolver::ResolveCurrent() const
    {
        const auto current = Current();
        if (!current) return {};
        const auto matches = [&](const RE::NiPointer<RE::Actor>& actor) {
            const auto identity = TryGetNpcIdentity(actor.get());
            return IsValidActor(actor.get()) && identity &&
                ResolvedReferenceMatches(current->identity, *identity);
        };
        if (auto actor = current->handle.get(); matches(actor)) return actor;
        if (!current->identity.IsPersistable()) return {};
        if (auto actor = index_.ResolveRuntime(current->ReferenceRuntimeID()); matches(actor)) return actor;
        if (auto actor = index_.Resolve(current->identity.StableReference()); matches(actor)) return actor;
        return {};
    }

    std::optional<SelectedTarget> TargetResolver::Current() const
    {
        std::scoped_lock lock(currentMutex_);
        return current_;
    }

    RE::NiPointer<RE::Actor> TargetResolver::ConsoleActor()
    {
        const auto reference = RE::Console::GetSelectedRef();
        auto* actor = reference ? reference->As<RE::Actor>() : nullptr;
        return IsValidActor(actor) ? actor->GetHandle().get() : RE::NiPointer<RE::Actor>{};
    }

    RE::NiPointer<RE::Actor> TargetResolver::CrosshairActor()
    {
        const auto* crosshair = RE::CrosshairPickData::GetSingleton();
        if (!crosshair) return {};

        for (const auto& handle : {crosshair->targetActor, crosshair->target}) {
            const auto reference = handle.get();
            auto* actor = reference ? reference->As<RE::Actor>() : nullptr;
            if (IsValidActor(actor)) return actor->GetHandle().get();
        }
        return {};
    }

    bool TargetResolver::IsValidActor(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        return IsEligibleNpcTarget(
            actor != nullptr,
            actor && actor->IsPlayerRef(),
            base != nullptr);
    }
}
