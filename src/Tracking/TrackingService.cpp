#include "PCH.h"

#include "Tracking/TrackingService.h"
#include "Tracking/QuestCleanupVerification.h"
#include "AppContext.h"
#include "Core/FormIdentityAdapter.h"
#include "Lifecycle/CallbackGuard.h"
#include "Lifecycle/ProcessContext.h"
#include "Persistence/Serialization.h"
#include "Tracking/TrackedDeathPolicy.h"
#include "UI/Localization.h"

#include <RE/F/FunctionArguments.h>

#include <algorithm>

namespace whereabouts
{
    namespace
    {
        constexpr std::string_view kQuestEditorId = "WhereaboutsTrackingQuest";
        constexpr std::string_view kPluginFile = "Whereabouts.esp";
        constexpr std::string_view kQuestScript = "WhereaboutsQuest";
        constexpr std::size_t kMaximumReadinessAttempts = 8;

        bool QuestCanDispatch(const RE::TESQuest& quest)
        {
            return quest.IsEnabled() && !quest.IsStopped() && !quest.IsStarting() && quest.IsRunning();
        }

        std::string TrackingQuestUnavailableError()
        {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            const bool pluginLoaded = dataHandler &&
                (dataHandler->LookupLoadedModByName(kPluginFile) ||
                 dataHandler->LookupLoadedLightModByName(kPluginFile));
            logger::warn(
                "Tracking quest lookup failed: plugin {}",
                pluginLoaded ? "is active but its quest record is missing" : "is not active");
            return std::string{TrackingQuestUnavailableMessage(pluginLoaded)};
        }

        void LogTrackingCallbackException() noexcept
        {
            try { logger::error("Contained exception in a tracking Papyrus callback"); }
            catch (...) {}
        }
        class PapyrusCompletionCallback final : public RE::BSScript::IStackCallbackFunctor
        {
        public:
            PapyrusCompletionCallback(
                TrackingService& service,
                TrackingCompletion completion,
                bool expectsBoolean) :
                service_(std::addressof(service)),
                completion_(std::move(completion)),
                expectsBoolean_(expectsBoolean)
            {}

            void operator()(RE::BSScript::Variable result) override
            {
                GuardCallbackVoid([this, result = std::move(result)]() mutable {
                auto* service = std::exchange(service_, nullptr);
                if (service) {
                    service->CompletePapyrusDispatch(
                        std::move(completion_), expectsBoolean_, std::move(result));
                }
                }, LogTrackingCallbackException);
            }

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

        private:
            TrackingService* service_;
            TrackingCompletion completion_;
            bool expectsBoolean_{false};
        };

        bool NotifyTrackedDeath(RE::StaticFunctionTag*)
        {
            return GuardCallback([] {
                auto* context = AcquireProcessContext();
                const auto settings = context ? context->runtimeSettings.Snapshot() : nullptr;
                return settings && settings->notifyTrackedDeath;
            }, false, LogTrackingCallbackException);
        }

        std::string FormatTrackedDeathNotification(
            RE::StaticFunctionTag*,
            std::string victimName,
            std::string killerName)
        {
            return GuardCallback([victimName = std::move(victimName), killerName = std::move(killerName)] {
                const auto displayName = victimName.empty() ?
                    ui::TranslateOwned("Tracked NPC") :
                    victimName;
                if (!killerName.empty()) {
                    return ui::TranslateFormat("{} was killed by {}.", displayName, killerName);
                }
                return ui::TranslateFormat("{} has died.", displayName);
            }, std::string{}, LogTrackingCallbackException);
        }

        bool RemoveTrackingOnDeath(RE::StaticFunctionTag*)
        {
            return false;
        }

        bool IsOperationEpochCurrent(RE::StaticFunctionTag*, std::int32_t epoch)
        {
            return GuardCallback([epoch] {
                auto* context = AcquireProcessContext();
                return context && context->operationEpoch.IsCurrent(OperationEpochToken{epoch});
            }, false, LogTrackingCallbackException);
        }

        void ReportMarkerState(
            RE::StaticFunctionTag*,
            std::int32_t objectiveIndex,
            bool questActive,
            bool objectiveDisplayed)
        {
            GuardCallbackVoid([=] {
                logger::info(
                    "Marker state: objective {}, quest active {}, displayed {}",
                    objectiveIndex,
                    questActive,
                    objectiveDisplayed);
            }, LogTrackingCallbackException);
        }

        void ReportTrackedDeathRemoved(RE::StaticFunctionTag*, RE::Actor* actor)
        {
            GuardCallbackVoid([actor] {
                if (auto* context = AcquireProcessContext()) {
                    context->tracking.HandleTrackedDeathRemoved(actor);
                }
            }, LogTrackingCallbackException);
        }

        void ReportTrackedDeath(
            RE::StaticFunctionTag*,
            RE::Actor* actor,
            std::int32_t objectiveIndex)
        {
            GuardCallbackVoid([actor, objectiveIndex] {
                if (auto* context = AcquireProcessContext()) {
                    context->tracking.HandleTrackedDeath(actor, objectiveIndex);
                }
            }, LogTrackingCallbackException);
        }

        std::string ActorLocationName(RE::Actor& actor)
        {
            if (const auto* location = actor.GetCurrentLocation()) {
                if (const auto* name = location->GetFullName(); name && *name != '\0') return name;
            }
            if (const auto* cell = actor.GetParentCell()) {
                if (const auto* name = cell->GetFullName(); name && *name != '\0') return name;
            }
            if (const auto* worldspace = actor.GetWorldspace()) {
                if (const auto* name = worldspace->GetFullName(); name && *name != '\0') return name;
            }
            return "Unknown";
        }

        bool TrackedEntryMatchesReference(
            const TrackedDeathEntry& entry,
            const RE::TESObjectREFR& reference)
        {
            const auto identity = TryGetFormIdentity(std::addressof(reference));
            return TrackedDeathReferenceMatches(
                entry.runtimeFormID,
                entry.identity,
                reference.GetFormID(),
                identity);
        }

        bool Dispatch(
            TrackingService& service,
            RE::TESQuest& quest,
            std::string_view method,
            RE::BSScript::IFunctionArguments* arguments,
            TrackingCompletion completion,
            bool expectsBoolean)
        {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
            if (!vm || !policy) return false;

            const auto handle = policy->GetHandleForObject(quest.GetFormType(), std::addressof(quest));
            if (handle == policy->EmptyHandle()) return false;

            // Mark only this quest's introductory HUD announcement as handled.
            // Objective display/force flags stay unchanged, preserving NPC notices.
            if (quest.data.flags.none(RE::QuestFlag::kDisplayedInHUD)) {
                quest.data.flags.set(RE::QuestFlag::kDisplayedInHUD);
                quest.AddChange(RE::TESQuest::ChangeFlags::kQuestFlags);
            }

            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> papyrusCallback;
            papyrusCallback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{
                new PapyrusCompletionCallback(service, std::move(completion), expectsBoolean)};
            return vm->DispatchMethodCall(
                handle,
                RE::BSFixedString(kQuestScript),
                RE::BSFixedString(method),
                arguments,
                papyrusCallback);
        }
    }

    TrackingService::TrackingService(
        SavedNpcStore& savedNpcs,
        OperationEpoch& operationEpoch,
        OperationQueue& operationQueue,
        UiCompletionMailbox& completions) noexcept :
        savedNpcs_(savedNpcs),
        operationEpoch_(operationEpoch),
        operationQueue_(operationQueue),
        completions_(completions)
    {}

    bool TrackingService::IsTracked(RE::Actor& actor) const
    {
        const auto* quest = ResolveQuest();
        if (!quest) return false;
        const auto layout = InspectLayout(*const_cast<RE::TESQuest*>(quest));
        if (!layout) return false;
        for (const auto* alias : *layout) {
            if (alias && alias->GetActorReference() == std::addressof(actor)) return true;
        }
        return false;
    }

    bool TrackingService::Busy() const
    {
        return ledger_.Busy();
    }

    std::expected<std::uint32_t, std::string> TrackingService::Track(
        RE::Actor& actor, OperationEpochToken token)
    {
        if (uninstallLocked_.load(std::memory_order_acquire)) {
            return std::unexpected("Whereabouts is prepared for uninstall");
        }
        auto* quest = ResolveQuest();
        if (!quest) return std::unexpected(TrackingQuestUnavailableError());
        const auto layout = InspectLayout(*quest);
        if (!layout) return std::unexpected(layout.error());

        std::optional<std::uint32_t> availableSlot;
        for (std::size_t slot = 0; slot < layout->size(); ++slot) {
            const auto* alias = (*layout)[slot];
            if (!alias) continue;
            if (alias->GetActorReference() == std::addressof(actor)) {
                const auto ticket = ledger_.TryBegin(
                    token,
                    TrackingOperation::Track,
                    actor.GetHandle().native_handle());
                if (!ticket) return std::unexpected("Tracking is updating");
                QueueActorDispatch(
                    actor.GetHandle(), "TrackActor", *ticket, actor.GetFormID(),
                    kMaximumReadinessAttempts);
                return static_cast<std::uint32_t>(slot);
            }
            if (!availableSlot && !alias->GetReference()) {
                availableSlot = static_cast<std::uint32_t>(slot);
            }
        }

        if (!availableSlot) return std::unexpected("All 100 tracking slots are in use");
        const auto ticket = ledger_.TryBegin(
            token,
            TrackingOperation::Track,
            actor.GetHandle().native_handle());
        if (!ticket) return std::unexpected("Tracking is updating");
        QueueActorDispatch(
            actor.GetHandle(), "TrackActor", *ticket, actor.GetFormID(), kMaximumReadinessAttempts);
        return *availableSlot;
    }

    std::expected<void, std::string> TrackingService::Untrack(
        RE::Actor& actor, OperationEpochToken token)
    {
        if (uninstallLocked_.load(std::memory_order_acquire)) {
            return std::unexpected("Whereabouts is prepared for uninstall");
        }
        auto* quest = ResolveQuest();
        if (!quest) return std::unexpected(TrackingQuestUnavailableError());
        if (!IsTracked(actor)) return std::unexpected("NPC is not tracked");
        const auto ticket = ledger_.TryBegin(
            token,
            TrackingOperation::Untrack,
            actor.GetHandle().native_handle());
        if (!ticket) return std::unexpected("Tracking is updating");
        QueueActorDispatch(
            actor.GetHandle(), "UntrackActor", *ticket, actor.GetFormID(), kMaximumReadinessAttempts);
        return {};
    }

    void TrackingService::RememberTrackedDeath(RE::Actor& actor, std::uint16_t objectiveIndex)
    {
        if (objectiveIndex >= kMaximumTrackedObjectives) return;
        const auto* displayName = actor.GetDisplayFullName();
        TrackedDeathEntry entry{
            actor.GetFormID(),
            TryGetFormIdentity(std::addressof(actor)),
            objectiveIndex,
            TrackedDeathState::BodyPresent,
            displayName && *displayName != '\0' ? displayName : "Tracked NPC",
            ActorLocationName(actor)};
        static_cast<void>(savedNpcs_.UpsertTrackedDeath(std::move(entry)));
    }

    void TrackingService::HandleTrackedDeath(RE::Actor* actor, std::int32_t objectiveIndex)
    {
        if (uninstallLocked_.load(std::memory_order_acquire)) return;
        const auto token = operationEpoch_.Capture();
        if (!token || !actor || !actor->IsDead() || objectiveIndex < 0 ||
            objectiveIndex >= kMaximumTrackedObjectives) return;
        const auto handle = actor->GetHandle();
        const auto runtimeFormID = actor->GetFormID();
        static_cast<void>(operationQueue_.SubmitGame(*token, [
            this, token = *token, handle, runtimeFormID, objectiveIndex] {
            const auto reference = handle.get();
            auto* current = reference ? skyrim_cast<RE::Actor*>(reference.get()) : nullptr;
            auto* quest = ResolveQuest();
            if (!current || !current->IsDead() || !quest) return;
            const auto layout = InspectLayout(*quest);
            if (!layout || (*layout)[objectiveIndex]->GetActorReference() != current) return;
            RememberTrackedDeath(*current, static_cast<std::uint16_t>(objectiveIndex));
            auto completion = TrackingCompletion{
                runtimeFormID,
                TrackingOperation::DeathObserved,
                true,
                "Tracked body remains marked.",
                token};
            completion.trackedRuntimeFormIDs = ActiveRuntimeIds();
            PublishCompletion(std::move(completion));
        }));
    }

    void TrackingService::HandleTrackedDeathRemoved(RE::Actor* actor)
    {
        if (uninstallLocked_.load(std::memory_order_acquire)) return;
        const auto token = operationEpoch_.Capture();
        if (!token || !actor) return;
        const auto handle = actor->GetHandle();
        const auto runtimeFormID = actor->GetFormID();
        static_cast<void>(operationQueue_.SubmitGame(*token, [
            this, token = *token, handle, runtimeFormID] {
            const auto reference = handle.get();
            auto* current = reference ? skyrim_cast<RE::Actor*>(reference.get()) : nullptr;
            if (!current) return;
            const auto* displayName = current->GetDisplayFullName();
            static_cast<void>(savedNpcs_.UpsertTrackedDeath({
                runtimeFormID,
                TryGetFormIdentity(current),
                kNoTrackedObjective,
                TrackedDeathState::BodyMissing,
                displayName && *displayName != '\0' ? displayName : "Tracked NPC",
                ActorLocationName(*current)}));
            PublishCompletion({
                runtimeFormID,
                TrackingOperation::DeathRemoval,
                true,
                "Tracking removed after NPC death.",
                token});
        }));
    }

    std::vector<RE::ObjectRefHandle> TrackingService::ActiveAliases() const
    {
        std::vector<RE::ObjectRefHandle> active;
        auto* quest = ResolveQuest();
        if (!quest) return active;
        const auto layout = InspectLayout(*quest);
        if (!layout) return active;
        active.reserve(layout->size());
        for (const auto* alias : *layout) {
            if (auto* reference = alias ? alias->GetReference() : nullptr) {
                active.push_back(reference->CreateRefHandle());
            }
        }
        return active;
    }

    std::expected<void, std::string> TrackingService::RefreshMarkers()
    {
        if (uninstallLocked_.load(std::memory_order_acquire)) return {};
        const auto token = operationEpoch_.Capture();
        if (!token) return std::unexpected("The current game session is not ready");
        return RefreshMarkers(*token);
    }

    std::expected<void, std::string> TrackingService::RefreshMarkers(OperationEpochToken token)
    {
        if (uninstallLocked_.load(std::memory_order_acquire)) return {};
        auto* quest = ResolveQuest();
        if (!quest) return std::unexpected(TrackingQuestUnavailableError());
        const auto layout = InspectLayout(*quest);
        if (!layout) return std::unexpected(layout.error());
        ReconcileTrackedDeaths(token);
        if (ActiveAliases().empty()) return {};
        const auto ticket = ledger_.TryBegin(token, TrackingOperation::Repair);
        if (!ticket) {
            ledger_.QueueRepair();
            return {};
        }
        QueueNoArgumentDispatch(
            "RefreshTrackedActors", *ticket, kMaximumReadinessAttempts);
        return {};
    }

    void TrackingService::ReconcileTrackedDeaths(OperationEpochToken token)
    {
        auto* quest = ResolveQuest();
        if (!quest) return;
        const auto layout = InspectLayout(*quest);
        if (!layout) return;

        for (std::size_t slot = 0; slot < layout->size(); ++slot) {
            auto* alias = (*layout)[slot];
            auto* actor = alias ? alias->GetActorReference() : nullptr;
            if (actor && actor->IsDead()) {
                RememberTrackedDeath(*actor, static_cast<std::uint16_t>(slot));
            }
        }

        const auto entries = savedNpcs_.TrackedDeaths();
        for (const auto& entry : entries) {
            if (entry.state == TrackedDeathState::BodyMissing ||
                entry.objectiveIndex >= kMaximumTrackedObjectives) {
                continue;
            }

            auto* alias = (*layout)[entry.objectiveIndex];
            auto* reference = alias ? alias->GetReference() : nullptr;
            const bool aliasOccupied = reference != nullptr;
            const bool aliasMatches = reference && TrackedEntryMatchesReference(entry, *reference);
            auto* actor = aliasMatches ? skyrim_cast<RE::Actor*>(reference) : nullptr;
            const auto action = DecideTrackedDeathAction({
                entry.state,
                aliasOccupied,
                aliasMatches,
                actor && !actor->IsDead(),
                actor && actor->IsDisabled()});

            if (action == TrackedDeathAction::KeepMarker) {
                if (actor) RememberTrackedDeath(*actor, entry.objectiveIndex);
                continue;
            }
            if (action == TrackedDeathAction::ForgetDeath) {
                static_cast<void>(savedNpcs_.RemoveTrackedDeath(entry));
                continue;
            }
            if (action == TrackedDeathAction::MarkMissingWithoutClearing) {
                MarkTrackedDeathMissing(entry.runtimeFormID);
                continue;
            }
            if (action == TrackedDeathAction::RetireEmptyMarker) {
                const auto ticket = ledger_.TryBegin(
                    token,
                    TrackingOperation::DeathRemoval,
                    std::nullopt,
                    entry.objectiveIndex);
                if (!ticket) {
                    ledger_.QueueRetirement(entry.objectiveIndex, 0, entry.runtimeFormID);
                    continue;
                }
                QueueEmptyObjectiveDispatch(
                    *ticket, entry.runtimeFormID, kMaximumReadinessAttempts);
                continue;
            }
            if (action != TrackedDeathAction::RetireMarker) continue;

            if (!reference) continue;
            const auto expectedHandle = reference->CreateRefHandle();
            const auto ticket = ledger_.TryBegin(
                token,
                TrackingOperation::DeathRemoval,
                expectedHandle.native_handle(),
                entry.objectiveIndex);
            if (!ticket) {
                ledger_.QueueRetirement(
                    entry.objectiveIndex,
                    expectedHandle.native_handle(),
                    entry.runtimeFormID);
                continue;
            }
            QueueObjectiveDispatch(
                expectedHandle, *ticket, entry.runtimeFormID, kMaximumReadinessAttempts);
        }
    }

    void TrackingService::MarkTrackedDeathMissing(std::uint32_t runtimeFormID)
    {
        const auto entries = savedNpcs_.TrackedDeaths();
        const auto existing = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.runtimeFormID == runtimeFormID && entry.state == TrackedDeathState::BodyPresent;
        });
        if (existing == entries.end()) return;
        if (std::count_if(entries.begin(), entries.end(), [&](const auto& entry) {
                return entry.runtimeFormID == runtimeFormID && entry.state == TrackedDeathState::BodyPresent;
            }) != 1) return;
        auto missing = *existing;
        missing.state = TrackedDeathState::BodyMissing;
        missing.objectiveIndex = kNoTrackedObjective;
        static_cast<void>(savedNpcs_.UpsertTrackedDeath(std::move(missing)));
    }

    void TrackingService::RemoveActiveTrackedDeaths()
    {
        auto retained = savedNpcs_.TrackedDeaths();
        std::erase_if(retained, [](const auto& entry) {
            return entry.state == TrackedDeathState::BodyPresent;
        });
        savedNpcs_.ReplaceTrackedDeaths(retained);
    }

    std::expected<void, std::string> TrackingService::ClearAll(OperationEpochToken token)
    {
        auto* quest = ResolveQuest();
        if (!quest) return std::unexpected(TrackingQuestUnavailableError());
        if (!InspectLayout(*quest)) return std::unexpected("Whereabouts tracking quest layout is invalid");
        const auto ticket = ledger_.TryBegin(token, TrackingOperation::ClearAll);
        if (!ticket) return std::unexpected("Tracking is updating");
        if (ActiveAliases().empty()) {
            HandleCompletion({
                0,
                TrackingOperation::ClearAll,
                true,
                "Tracking clear completed.",
                ticket->epoch,
                ticket->serial,
                ticket->objectiveIndex});
            return {};
        }
        QueueNoArgumentDispatch(
            "ClearTrackedActors", *ticket, kMaximumReadinessAttempts);
        return {};
    }

    std::expected<void, std::string> TrackingService::PrepareForUninstall(OperationEpochToken token)
    {
        auto* quest = ResolveQuest();
        if (!quest) return std::unexpected(TrackingQuestUnavailableError());
        if (!InspectLayout(*quest)) return std::unexpected("Whereabouts tracking quest layout is invalid");
        ledger_.Invalidate();
        const auto ticket = ledger_.TryBegin(token, TrackingOperation::PrepareForUninstall);
        if (!ticket) return std::unexpected("Uninstall cleanup could not acquire tracking ownership");
        if (ActiveAliases().empty()) {
            QueueStopAndReset(
                *ticket,
                kMaximumReadinessAttempts,
                false,
                TrackingCompletion{
                    0,
                    TrackingOperation::PrepareForUninstall,
                    true,
                    "Whereabouts cleanup completed.",
                    ticket->epoch,
                    ticket->serial,
                    ticket->objectiveIndex});
            return {};
        }
        QueueNoArgumentDispatch(
            "ClearTrackedActors",
            *ticket,
            kMaximumReadinessAttempts);
        return {};
    }

    void TrackingService::CancelPendingOperations() noexcept
    {
        try {
            ledger_.Invalidate();
        } catch (...) {
            try { logger::error("Could not clear pending tracking operation state"); }
            catch (...) {}
        }
    }

    std::expected<void, std::string> TrackingService::Resume(OperationEpochToken token)
    {
        auto* quest = ResolveQuest();
        if (!quest) return std::unexpected(TrackingQuestUnavailableError());
        if (!InspectLayout(*quest)) return std::unexpected("Whereabouts tracking quest layout is invalid");
        const auto ticket = ledger_.TryBegin(token, TrackingOperation::Resume);
        if (!ticket) return std::unexpected("Tracking is updating");
        QueueNoArgumentDispatch("RefreshTrackedActors", *ticket, kMaximumReadinessAttempts);
        return {};
    }

    RE::TESQuest* TrackingService::ResolveQuest() const
    {
        return RE::TESForm::LookupByEditorID<RE::TESQuest>(kQuestEditorId);
    }

    std::expected<std::array<RE::BGSRefAlias*, kTrackingSlotCount>, std::string>
        TrackingService::InspectLayout(RE::TESQuest& quest) const
    {
        TrackingQuestLayoutInput input;
        input.aliases.reserve(quest.aliases.size());
        for (const auto* alias : quest.aliases) {
            input.aliases.push_back({
                alias ? alias->aliasID : 0U,
                alias && skyrim_cast<const RE::BGSRefAlias*>(alias) != nullptr});
        }
        for (const auto* objective : quest.objectives) {
            if (objective) input.objectives.push_back(objective->index);
        }

        const auto validated = ValidateTrackingQuestLayout(input);
        if (!validated) return std::unexpected(validated.error());

        std::array<RE::BGSRefAlias*, kTrackingSlotCount> aliases{};
        for (std::size_t slot = 0; slot < aliases.size(); ++slot) {
            aliases[slot] = skyrim_cast<RE::BGSRefAlias*>(
                quest.aliases[validated->aliasVectorIndices[slot]]);
            if (!aliases[slot]) return std::unexpected("A validated tracking alias became unavailable");
        }
        return aliases;
    }

    bool TrackingService::IsQuestScriptBound(RE::TESQuest& quest) const
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!vm || !policy) return false;
        const auto handle = policy->GetHandleForObject(quest.GetFormType(), std::addressof(quest));
        if (handle == policy->EmptyHandle()) return false;
        RE::BSTSmartPointer<RE::BSScript::Object> object;
        return vm->FindBoundObject(handle, kQuestScript.data(), object) && object;
    }

    bool TrackingService::EnsureQuestStarted(
        RE::TESQuest& quest,
        TrackingLifecycleEvent event,
        const TrackingOperationTicket& ticket,
        std::uint32_t runtimeFormID)
    {
        const auto activeAliases = ActiveAliases().size();
        const TrackingLifecycleState state{
            .questRunning = quest.IsEnabled() && quest.IsRunning(),
            .scriptBound = IsQuestScriptBound(quest),
            .activeAliases = activeAliases,
            .questStarting = quest.IsStarting(),
            .uninstallLocked = uninstallLocked_.load(std::memory_order_acquire),
            .questStopped = quest.IsStopped()};
        const auto action = DecideTrackingLifecycle(state, event);
        if (action != TrackingLifecycleAction::StartQuest) return action != TrackingLifecycleAction::None;

        logger::info(
            "Tracking quest start requested: operation {}, FormID {:08X}, reason {}, running {}, starting {}, active aliases {}",
            static_cast<int>(ticket.operation),
            runtimeFormID,
            static_cast<int>(event),
            state.questRunning,
            state.questStarting,
            state.activeAliases);
        if (!operationEpoch_.IsCurrent(ticket.epoch)) {
            logger::warn("Tracking quest start cancelled by stale operation epoch");
            return false;
        }
        const bool started = quest.Start();
        logger::info(
            "Tracking quest start result: operation {}, FormID {:08X}, started {}, running {}, starting {}",
            static_cast<int>(ticket.operation),
            runtimeFormID,
            started,
            quest.IsRunning(),
            quest.IsStarting());
        return started || QuestCanDispatch(quest) || quest.IsStarting();
    }

    void TrackingService::SetUninstallLocked(bool locked) noexcept
    {
        uninstallLocked_.store(locked, std::memory_order_release);
    }

    void TrackingService::QueueActorDispatch(
        RE::ObjectRefHandle actorHandle,
        std::string method,
        TrackingOperationTicket ticket,
        std::uint32_t runtimeFormID,
        std::size_t attemptsRemaining)
    {
        if (!operationEpoch_.IsCurrent(ticket.epoch)) return;
        auto* quest = ResolveQuest();
        const auto reference = actorHandle.get();
        auto* actor = reference ? skyrim_cast<RE::Actor*>(reference.get()) : nullptr;
        if (!quest || !actor || actorHandle.native_handle() != ticket.referenceHandle.value_or(0)) {
            ReportFailure(ticket, runtimeFormID, "Tracking target is unavailable");
            return;
        }
        if (!InspectLayout(*quest)) {
            ReportFailure(ticket, runtimeFormID, "Tracking quest layout is invalid");
            return;
        }

        if (!QuestCanDispatch(*quest)) {
            const auto event = ticket.operation == TrackingOperation::Track ?
                TrackingLifecycleEvent::TrackRequested : TrackingLifecycleEvent::ReadinessTick;
            if (!EnsureQuestStarted(*quest, event, ticket, runtimeFormID)) {
                ReportFailure(ticket, runtimeFormID, "Tracking quest could not be started");
                return;
            }
        } else if (IsQuestScriptBound(*quest)) {
            if (!DispatchActorMethod(*quest, method, *actor, ticket, runtimeFormID)) {
                ReportFailure(ticket, runtimeFormID, "Tracking request could not be dispatched");
            }
            return;
        }

        if (attemptsRemaining == 0) {
            ReportFailure(ticket, runtimeFormID, "Tracking quest script did not become ready");
            return;
        }
        const auto submitted = operationQueue_.SubmitGame(ticket.epoch, [
                this,
                actorHandle,
                method = std::move(method),
                ticket,
                runtimeFormID,
                attemptsRemaining] () mutable {
                QueueActorDispatch(
                    actorHandle, std::move(method), ticket, runtimeFormID, attemptsRemaining - 1);
            });
        if (!submitted) ReportFailure(ticket, runtimeFormID, "SKSE task interface is unavailable");
    }

    void TrackingService::QueueNoArgumentDispatch(
        std::string method,
        TrackingOperationTicket ticket,
        std::size_t attemptsRemaining)
    {
        if (!operationEpoch_.IsCurrent(ticket.epoch)) return;
        auto* quest = ResolveQuest();
        if (!quest) {
            ReportFailure(ticket, 0, "Tracking quest is unavailable");
            return;
        }
        if (!InspectLayout(*quest)) {
            ReportFailure(ticket, 0, "Tracking quest layout is invalid");
            return;
        }
        if (!QuestCanDispatch(*quest)) {
            const auto event = ticket.operation == TrackingOperation::Repair ?
                TrackingLifecycleEvent::PostLoadRepair : TrackingLifecycleEvent::ReadinessTick;
            if (!EnsureQuestStarted(*quest, event, ticket, 0)) {
                ReportFailure(ticket, 0, "Tracking quest could not be started");
                return;
            }
        } else if (IsQuestScriptBound(*quest)) {
            if (!DispatchNoArgumentMethod(*quest, method, ticket)) {
                ReportFailure(ticket, 0, "Tracking request could not be dispatched");
            }
            return;
        }

        if (attemptsRemaining == 0) {
            ReportFailure(ticket, 0, "Tracking quest script did not become ready");
            return;
        }
        const auto submitted = operationQueue_.SubmitGame(ticket.epoch, [
            this, method = std::move(method), ticket, attemptsRemaining]() mutable {
            QueueNoArgumentDispatch(std::move(method), ticket, attemptsRemaining - 1);
        });
        if (!submitted) ReportFailure(ticket, 0, "SKSE task interface is unavailable");
    }

    void TrackingService::QueueObjectiveDispatch(
        RE::ObjectRefHandle expectedReference,
        TrackingOperationTicket ticket,
        std::uint32_t runtimeFormID,
        std::size_t attemptsRemaining)
    {
        if (!operationEpoch_.IsCurrent(ticket.epoch)) return;
        auto* quest = ResolveQuest();
        const auto reference = expectedReference.get();
        if (!quest || !reference ||
            expectedReference.native_handle() != ticket.referenceHandle.value_or(0)) {
            ReportFailure(ticket, runtimeFormID, "Tracking quest or expected body is unavailable");
            return;
        }
        const auto layout = InspectLayout(*quest);
        if (!layout || !ticket.objectiveIndex || *ticket.objectiveIndex >= layout->size() ||
            (*layout)[*ticket.objectiveIndex]->GetReference() != reference.get()) {
            ReportFailure(ticket, runtimeFormID, "Tracking slot no longer owns the expected body");
            return;
        }
        if (!QuestCanDispatch(*quest)) {
            if (!EnsureQuestStarted(
                    *quest, TrackingLifecycleEvent::ReadinessTick, ticket, runtimeFormID)) {
                ReportFailure(ticket, runtimeFormID, "Tracking quest could not be started");
                return;
            }
        } else if (IsQuestScriptBound(*quest)) {
            if (!DispatchObjectiveMethod(*quest, *reference, ticket, runtimeFormID)) {
                ReportFailure(ticket, runtimeFormID, "Dead-body retirement could not be dispatched");
            }
            return;
        }

        if (attemptsRemaining == 0) {
            ReportFailure(
                ticket, runtimeFormID,
                "Tracking quest script did not become ready for dead-body retirement");
            return;
        }
        const auto submitted = operationQueue_.SubmitGame(ticket.epoch, [
            this, expectedReference, ticket, runtimeFormID, attemptsRemaining] {
            QueueObjectiveDispatch(
                expectedReference, ticket, runtimeFormID, attemptsRemaining - 1);
        });
        if (!submitted) ReportFailure(ticket, runtimeFormID, "SKSE task interface is unavailable");
    }

    void TrackingService::QueueEmptyObjectiveDispatch(
        TrackingOperationTicket ticket,
        std::uint32_t runtimeFormID,
        std::size_t attemptsRemaining)
    {
        if (!operationEpoch_.IsCurrent(ticket.epoch)) return;
        auto* quest = ResolveQuest();
        if (!quest) {
            ReportFailure(ticket, runtimeFormID, "Tracking quest is unavailable");
            return;
        }
        const auto layout = InspectLayout(*quest);
        if (!layout || !ticket.objectiveIndex || *ticket.objectiveIndex >= layout->size()) {
            ReportFailure(ticket, runtimeFormID, "Tracking objective is unavailable");
            return;
        }
        if ((*layout)[*ticket.objectiveIndex]->GetReference()) {
            MarkTrackedDeathMissing(runtimeFormID);
            if (ledger_.Complete(ticket)) DrainDeferred(ticket.epoch);
            return;
        }

        if (!QuestCanDispatch(*quest)) {
            if (!EnsureQuestStarted(
                    *quest, TrackingLifecycleEvent::ReadinessTick, ticket, runtimeFormID)) {
                ReportFailure(ticket, runtimeFormID, "Tracking quest could not be started");
                return;
            }
        } else if (IsQuestScriptBound(*quest)) {
            if (!DispatchEmptyObjectiveMethod(*quest, ticket, runtimeFormID)) {
                ReportFailure(ticket, runtimeFormID, "Empty tracked objective could not be dispatched");
            }
            return;
        }

        if (attemptsRemaining == 0) {
            ReportFailure(
                ticket, runtimeFormID,
                "Tracking quest script did not become ready for empty-objective retirement");
            return;
        }
        const auto submitted = operationQueue_.SubmitGame(ticket.epoch, [
            this, ticket, runtimeFormID, attemptsRemaining] {
            QueueEmptyObjectiveDispatch(ticket, runtimeFormID, attemptsRemaining - 1);
        });
        if (!submitted) ReportFailure(ticket, runtimeFormID, "SKSE task interface is unavailable");
    }

    void TrackingService::HandleCompletion(TrackingCompletion completion)
    {
        const TrackingOperationTicket ticket{
            completion.epoch,
            completion.requestSerial,
            completion.operation,
            completion.referenceHandle,
            completion.objectiveIndex};
        if (!operationEpoch_.IsCurrent(ticket.epoch)) return;

        if (completion.succeeded && completion.operation == TrackingOperation::Track) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(completion.runtimeFormID);
            auto* quest = ResolveQuest();
            const auto layout = quest ? InspectLayout(*quest) :
                std::expected<std::array<RE::BGSRefAlias*, kTrackingSlotCount>, std::string>{
                    std::unexpected("Tracking quest is unavailable")};
            std::optional<std::uint16_t> authoritativeSlot;
            if (actor && layout) {
                for (std::size_t slot = 0; slot < layout->size(); ++slot) {
                    if ((*layout)[slot]->GetActorReference() == actor) {
                        authoritativeSlot = static_cast<std::uint16_t>(slot);
                        break;
                    }
                }
            }
            if (!authoritativeSlot) {
                completion.succeeded = false;
                completion.message = "Track did not acquire a tracking alias.";
            } else {
                completion.objectiveIndex = authoritativeSlot;
                bool displayed = false;
                for (const auto* objective : quest->objectives) {
                    if (objective && objective->index == *authoritativeSlot) {
                        displayed = objective->state.get() == RE::QUEST_OBJECTIVE_STATE::kDisplayed;
                        break;
                    }
                }
                if (!QuestCanDispatch(*quest) || !quest->IsActive() || !displayed) {
                    completion.succeeded = false;
                    completion.message = "Tracking alias exists, but its active map objective could not be verified. Try Track again.";
                } else if (actor->IsDead()) RememberTrackedDeath(*actor, *authoritativeSlot);
            }
        }
        if (completion.operation == TrackingOperation::Resume) {
            auto* quest = ResolveQuest();
            completion.succeeded = completion.succeeded && quest &&
                QuestCanDispatch(*quest) && IsQuestScriptBound(*quest);
            completion.message = completion.succeeded ?
                "Whereabouts resumed. Tracking is ready; saved lists remain empty." :
                "Tracking could not resume. Whereabouts remains locked; retry Resume or reload a save from before cleanup.";
        }
        if (completion.succeeded && completion.operation == TrackingOperation::Untrack) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(completion.runtimeFormID);
            if (actor && IsTracked(*actor)) {
                completion.succeeded = false;
                completion.message = "Untrack did not release the tracking alias.";
            } else {
                static_cast<void>(savedNpcs_.RemoveTrackedDeath(completion.runtimeFormID));
            }
        }
        if (completion.succeeded && completion.operation == TrackingOperation::ClearAll) {
            RemoveActiveTrackedDeaths();
        }
        if (completion.succeeded && completion.operation == TrackingOperation::DeathRemoval) {
            MarkTrackedDeathMissing(completion.runtimeFormID);
        }
        if (completion.operation == TrackingOperation::PrepareForUninstall) {
            if (!completion.succeeded) {
                if (ledger_.Complete(ticket)) PublishCompletion(std::move(completion));
                return;
            }
            if (!ActiveAliases().empty()) {
                completion.succeeded = false;
                completion.message = "Uninstall cleanup did not clear every tracking alias.";
                if (ledger_.Complete(ticket)) PublishCompletion(std::move(completion));
                return;
            }
            QueueStopAndReset(ticket, kMaximumReadinessAttempts, 0, std::move(completion));
            return;
        }
        if (!ledger_.Complete(ticket)) return;
        if (completion.succeeded) {
            completion.trackedRuntimeFormIDs = ActiveRuntimeIds();
        }
        PublishCompletion(std::move(completion));
        DrainDeferred(ticket.epoch);
    }

    void TrackingService::QueueStopAndReset(
        TrackingOperationTicket ticket,
        std::size_t attemptsRemaining,
        std::uint8_t phase,
        std::optional<TrackingCompletion> completion)
    {
        if (!operationEpoch_.IsCurrent(ticket.epoch)) return;
        if (!ActiveAliases().empty()) {
            ReportFailure(ticket, 0, "Uninstall cleanup found an active tracking alias.");
            return;
        }
        auto* quest = ResolveQuest();
        if (!quest) {
            ReportFailure(ticket, 0, TrackingQuestUnavailableError());
            return;
        }
        if (!InspectLayout(*quest)) {
            ReportFailure(ticket, 0, "Whereabouts tracking quest layout is invalid.");
            return;
        }

        if (phase == 0) {
            if (!operationEpoch_.IsCurrent(ticket.epoch)) return;
            if (!quest->IsStopped()) quest->Stop();
            phase = 1;
        } else if (phase == 1 && quest->IsStopped()) {
            if (!operationEpoch_.IsCurrent(ticket.epoch)) return;
            quest->Reset();
            logger::info("Tracking quest stopped and reset during Prepare for Uninstall");
            phase = 2;
        } else if (phase == 2) {
            bool objectivesHidden = true;
            for (const auto* objective : quest->objectives) {
                if (!objective) continue;
                const auto state = objective->state.get();
                if (state == RE::QUEST_OBJECTIVE_STATE::kDisplayed ||
                    state == RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed ||
                    state == RE::QUEST_OBJECTIVE_STATE::kFailedDisplayed) {
                    objectivesHidden = false;
                    break;
                }
            }
            const QuestCleanupObservation observation{
                .stopped = quest->IsStopped(),
                .running = quest->IsRunning(),
                .activeAliasCount = ActiveAliases().size(),
                .objectivesHidden = objectivesHidden};
            if (IsQuestCleanupVerified(observation)) {
                logger::info(
                    "Prepare for Uninstall quest verification passed: stopped {}, running {}, aliases {}, objectives hidden {}",
                    observation.stopped,
                    observation.running,
                    observation.activeAliasCount,
                    observation.objectivesHidden);
                if (completion) {
                    completion->succeeded = true;
                    completion->message = "Whereabouts cleanup completed.";
                    if (ledger_.Complete(ticket)) PublishCompletion(std::move(*completion));
                }
                return;
            }
            if (attemptsRemaining == 0) {
                logger::error(
                    "Prepare for Uninstall quest verification failed: stopped {}, running {}, aliases {}, objectives hidden {}",
                    observation.stopped,
                    observation.running,
                    observation.activeAliasCount,
                    observation.objectivesHidden);
            }
        }

        if (attemptsRemaining == 0) {
            ReportFailure(
                ticket,
                0,
                phase == 1 ?
                    "Whereabouts tracking quest did not stop; do not uninstall yet." :
                    "Whereabouts quest cleanup could not be verified; do not uninstall yet.");
            return;
        }
        const auto submitted = operationQueue_.SubmitGame(ticket.epoch, [
            this, ticket, attemptsRemaining, phase, completion = std::move(completion)]() mutable {
            QueueStopAndReset(ticket, attemptsRemaining - 1, phase, std::move(completion));
        });
        if (!submitted) ReportFailure(
            ticket, 0, "SKSE task interface is unavailable; do not uninstall yet.");
    }

    void TrackingService::ReportFailure(
        const TrackingOperationTicket& ticket,
        std::uint32_t runtimeFormID,
        std::string message)
    {
        logger::error(
            "Tracking failure: operation {}, FormID {:08X}, {}",
            static_cast<int>(ticket.operation),
            runtimeFormID,
            message);
        if (!ledger_.Complete(ticket)) return;
        PublishCompletion({
            runtimeFormID,
            ticket.operation,
            false,
            std::move(message),
            ticket.epoch,
            ticket.serial,
            ticket.objectiveIndex,
            ticket.referenceHandle});
        DrainDeferred(ticket.epoch);
    }

    void TrackingService::PublishCompletion(TrackingCompletion completion)
    {
        if (operationEpoch_.IsCurrent(completion.epoch)) {
            completions_.PushTracking(std::move(completion));
        }
    }

    void TrackingService::CompletePapyrusDispatch(
        TrackingCompletion completion,
        bool expectsBoolean,
        RE::BSScript::Variable result)
    {
        completion.succeeded = !expectsBoolean || (result.IsBool() && result.GetBool());
        completion.message += completion.succeeded ? " completed." : " did not complete.";
        logger::info(
            "Tracking completion: operation {}, FormID {:08X}, success {}",
            static_cast<int>(completion.operation),
            completion.runtimeFormID,
            completion.succeeded);
        const auto token = completion.epoch;
        auto failedSubmission = completion;
        const auto submitted = operationQueue_.SubmitGame(token, [
            this, completion = std::move(completion)]() mutable {
            HandleCompletion(std::move(completion));
        });
        if (!submitted) {
            const TrackingOperationTicket ticket{
                token,
                failedSubmission.requestSerial,
                failedSubmission.operation,
                failedSubmission.referenceHandle,
                failedSubmission.objectiveIndex};
            if (ledger_.Complete(ticket)) {
                failedSubmission.succeeded = false;
                failedSubmission.message = "The game session ended before tracking completed.";
                PublishCompletion(std::move(failedSubmission));
            }
        }
    }

    void TrackingService::DrainDeferred(OperationEpochToken token)
    {
        if (!operationEpoch_.IsCurrent(token)) return;
        auto retirements = ledger_.TakeRetirements();
        auto* quest = ResolveQuest();
        const auto layout = quest ? InspectLayout(*quest) :
            std::expected<std::array<RE::BGSRefAlias*, kTrackingSlotCount>, std::string>{
                std::unexpected("Tracking quest is unavailable")};
        if (!layout) return;
        for (std::size_t index = 0; index < retirements.size(); ++index) {
            const auto& retirement = retirements[index];
            if (retirement.objectiveIndex >= layout->size()) continue;
            auto* reference = (*layout)[retirement.objectiveIndex]->GetReference();
            if (retirement.referenceHandle == 0) {
                if (reference) {
                    MarkTrackedDeathMissing(retirement.runtimeFormID);
                    continue;
                }
                const auto ticket = ledger_.TryBegin(
                    token,
                    TrackingOperation::DeathRemoval,
                    std::nullopt,
                    retirement.objectiveIndex);
                if (!ticket) continue;
                for (std::size_t pending = index + 1; pending < retirements.size(); ++pending) {
                    ledger_.QueueRetirement(
                        retirements[pending].objectiveIndex,
                        retirements[pending].referenceHandle,
                        retirements[pending].runtimeFormID);
                }
                QueueEmptyObjectiveDispatch(
                    *ticket, retirement.runtimeFormID, kMaximumReadinessAttempts);
                return;
            }
            if (!reference) continue;
            const auto handle = reference->CreateRefHandle();
            if (handle.native_handle() != retirement.referenceHandle) continue;
            const auto ticket = ledger_.TryBegin(
                token,
                TrackingOperation::DeathRemoval,
                retirement.referenceHandle,
                retirement.objectiveIndex);
            if (!ticket) continue;
            for (std::size_t pending = index + 1; pending < retirements.size(); ++pending) {
                ledger_.QueueRetirement(
                    retirements[pending].objectiveIndex,
                    retirements[pending].referenceHandle,
                    retirements[pending].runtimeFormID);
            }
            QueueObjectiveDispatch(
                handle, *ticket, retirement.runtimeFormID, kMaximumReadinessAttempts);
            return;
        }
        if (ledger_.TakeRepair()) static_cast<void>(RefreshMarkers(token));
    }

    std::vector<std::uint32_t> TrackingService::ActiveRuntimeIds() const
    {
        const auto handles = ActiveAliases();
        std::vector<std::uint32_t> runtimeFormIDs;
        runtimeFormIDs.reserve(handles.size());
        for (const auto& handle : handles) {
            if (const auto reference = handle.get()) {
                runtimeFormIDs.push_back(reference->GetFormID());
            }
        }
        return runtimeFormIDs;
    }

    bool TrackingService::DispatchActorMethod(
        RE::TESQuest& quest,
        std::string_view method,
        RE::Actor& actor,
        const TrackingOperationTicket& ticket,
        std::uint32_t runtimeFormID)
    {
        logger::info(
            "Tracking request: operation {}, FormID {:08X}",
            static_cast<int>(ticket.operation),
            actor.GetFormID());
        return Dispatch(
            *this,
            quest,
            method,
            RE::MakeFunctionArguments(
                std::addressof(actor), std::int32_t{ticket.epoch.value}),
            TrackingCompletion{
                runtimeFormID,
                ticket.operation,
                false,
                ticket.operation == TrackingOperation::Track ? "Track" : "Untrack",
                ticket.epoch,
                ticket.serial,
                ticket.objectiveIndex,
                ticket.referenceHandle},
            true);
    }

    bool TrackingService::DispatchNoArgumentMethod(
        RE::TESQuest& quest,
        std::string_view method,
        const TrackingOperationTicket& ticket)
    {
        logger::info("Tracking request: operation {}, FormID 00000000", static_cast<int>(ticket.operation));
        const auto* message = ticket.operation == TrackingOperation::PrepareForUninstall ?
            "Uninstall cleanup" : "Tracking clear";
        return Dispatch(
            *this,
            quest,
            method,
            RE::MakeFunctionArguments(std::int32_t{ticket.epoch.value}),
            TrackingCompletion{
                0,
                ticket.operation,
                false,
                message,
                ticket.epoch,
                ticket.serial,
                ticket.objectiveIndex,
                ticket.referenceHandle},
            true);
    }

    bool TrackingService::DispatchObjectiveMethod(
        RE::TESQuest& quest,
        RE::TESObjectREFR& expectedReference,
        const TrackingOperationTicket& ticket,
        std::uint32_t runtimeFormID)
    {
        logger::info(
            "Dead-body retirement request: objective {}, FormID {:08X}",
            ticket.objectiveIndex.value_or(kNoTrackedObjective),
            runtimeFormID);
        return Dispatch(
            *this,
            quest,
            "RetireTrackedObjective",
            RE::MakeFunctionArguments(
                static_cast<std::int32_t>(ticket.objectiveIndex.value_or(kNoTrackedObjective)),
                std::addressof(expectedReference),
                std::int32_t{ticket.epoch.value}),
            TrackingCompletion{
                runtimeFormID,
                TrackingOperation::DeathRemoval,
                false,
                "Tracked body is no longer available.",
                ticket.epoch,
                ticket.serial,
                ticket.objectiveIndex,
                ticket.referenceHandle},
            true);
    }

    bool TrackingService::DispatchEmptyObjectiveMethod(
        RE::TESQuest& quest,
        const TrackingOperationTicket& ticket,
        std::uint32_t runtimeFormID)
    {
        logger::info(
            "Empty dead-body objective retirement request: objective {}, FormID {:08X}",
            ticket.objectiveIndex.value_or(kNoTrackedObjective),
            runtimeFormID);
        return Dispatch(
            *this,
            quest,
            "RetireEmptyTrackedObjective",
            RE::MakeFunctionArguments(
                static_cast<std::int32_t>(ticket.objectiveIndex.value_or(kNoTrackedObjective)),
                std::int32_t{ticket.epoch.value}),
            TrackingCompletion{
                runtimeFormID,
                TrackingOperation::DeathRemoval,
                false,
                "Tracked body is no longer available.",
                ticket.epoch,
                ticket.serial,
                ticket.objectiveIndex,
                ticket.referenceHandle},
            true);
    }

    bool RegisterTrackingPapyrus(RE::BSScript::IVirtualMachine* vm)
    {
        if (!vm) return false;
        vm->RegisterFunction("IsOperationEpochCurrent", "WhereaboutsNative", IsOperationEpochCurrent);
        vm->RegisterFunction("NotifyTrackedDeath", "WhereaboutsNative", NotifyTrackedDeath);
        vm->RegisterFunction(
            "FormatTrackedDeathNotification",
            "WhereaboutsNative",
            FormatTrackedDeathNotification);
        vm->RegisterFunction("RemoveTrackingOnDeath", "WhereaboutsNative", RemoveTrackingOnDeath);
        vm->RegisterFunction("ReportMarkerState", "WhereaboutsNative", ReportMarkerState);
        vm->RegisterFunction("ReportTrackedDeath", "WhereaboutsNative", ReportTrackedDeath);
        vm->RegisterFunction(
            "ReportTrackedDeathRemoved", "WhereaboutsNative", ReportTrackedDeathRemoved);
        return true;
    }
}
