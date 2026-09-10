#include "PCH.h"

#include "Commands/CommandService.h"
#include "Core/FormIdentityAdapter.h"
#include "Lifecycle/CallbackGuard.h"
#include "Persistence/Serialization.h"
#include "Search/RuntimeIndex.h"
#include "SKSEMenuFramework.h"
#include "Tracking/TrackingService.h"

#include <RE/F/FunctionArguments.h>

#include <algorithm>
#include <ranges>

namespace whereabouts
{
    namespace
    {
        void LogCommandCallbackException() noexcept
        {
            try { logger::error("Contained exception in a command Papyrus completion"); }
            catch (...) {}
        }

        class MovementCompletionCallback final : public RE::BSScript::IStackCallbackFunctor
        {
        public:
            MovementCompletionCallback(
                RuntimeIndex& index,
                MovementRequestGate& gate,
                OperationQueue& queue,
                OperationEpochToken operationToken,
                std::uint64_t requestToken,
                std::uint32_t runtimeFormID,
                std::string command) :
                index_(std::addressof(index)),
                gate_(std::addressof(gate)),
                queue_(std::addressof(queue)),
                operationToken_(operationToken),
                requestToken_(requestToken),
                runtimeFormID_(runtimeFormID),
                command_(std::move(command))
            {}

            void operator()(RE::BSScript::Variable result) override
            {
                GuardCallbackVoid([this, result = std::move(result)]() mutable {
                if (!gate_->Complete(requestToken_)) {
                    logger::info(
                        "Ignored stale movement completion: {}, FormID {:08X}, request {}",
                        command_,
                        runtimeFormID_,
                        requestToken_);
                    return;
                }
                const bool succeeded = result.IsBool() && result.GetBool();
                logger::info(
                    "Movement completion: {}, FormID {:08X}, success {}",
                    command_,
                    runtimeFormID_,
                    succeeded);
                if (!succeeded) return;
                static_cast<void>(queue_->SubmitGame(
                    operationToken_,
                    [index = index_, runtimeFormID = runtimeFormID_] {
                        static_cast<void>(index->RefreshRuntimeId(runtimeFormID));
                    }));
                }, LogCommandCallbackException);
            }

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

        private:
            RuntimeIndex* index_;
            MovementRequestGate* gate_;
            OperationQueue* queue_;
            OperationEpochToken operationToken_;
            std::uint64_t requestToken_;
            std::uint32_t runtimeFormID_;
            std::string command_;
        };

        class EnabledStateCompletionCallback final : public RE::BSScript::IStackCallbackFunctor
        {
        public:
            EnabledStateCompletionCallback(
                std::uint32_t runtimeFormID,
                bool expectedEnabled,
                std::uint64_t requestSerial,
                OperationEpochToken operationToken,
                RequestSerialGate& gate,
                OperationQueue& queue,
                UiCompletionMailbox& completions) :
                runtimeFormID_(runtimeFormID),
                expectedEnabled_(expectedEnabled),
                requestSerial_(requestSerial),
                operationToken_(operationToken),
                gate_(std::addressof(gate)),
                queue_(std::addressof(queue)),
                completions_(std::addressof(completions))
            {}

            void operator()(RE::BSScript::Variable) override
            {
                GuardCallbackVoid([this] {
                logger::info(
                    "Papyrus Enable/Disable completed: FormID {:08X}, expected enabled {}",
                    runtimeFormID_,
                    expectedEnabled_);
                if (!gate_->IsCurrent(requestSerial_)) return;
                static_cast<void>(queue_->SubmitGame(operationToken_, [
                    runtimeFormID = runtimeFormID_,
                    expectedEnabled = expectedEnabled_,
                    operationToken = operationToken_,
                    requestSerial = requestSerial_,
                    gate = gate_,
                    completions = completions_] {
                    if (!gate->IsCurrent(requestSerial)) return;
                    completions->PushEnabled({
                        runtimeFormID,
                        expectedEnabled,
                        operationToken,
                        requestSerial});
                }));
                }, LogCommandCallbackException);
            }

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

        private:
            std::uint32_t runtimeFormID_;
            bool expectedEnabled_;
            std::uint64_t requestSerial_;
            OperationEpochToken operationToken_;
            RequestSerialGate* gate_;
            OperationQueue* queue_;
            UiCompletionMailbox* completions_;
        };

        MovementBoundary LiveMovementBoundary(const RE::Actor& actor, const RE::PlayerCharacter& player)
        {
            const auto* actorWorldspace = actor.GetWorldspace();
            const auto* playerWorldspace = player.GetWorldspace();
            const auto* actorCell = actor.GetParentCell();
            const auto* playerCell = player.GetParentCell();
            return ClassifyMovementBoundary(
                actorWorldspace != nullptr,
                playerWorldspace != nullptr,
                actorWorldspace != nullptr && actorWorldspace == playerWorldspace,
                actorCell != nullptr,
                playerCell != nullptr,
                actorCell != nullptr && actorCell == playerCell);
        }

        std::expected<void, std::string> RequireBoundaryConfirmation(
            const RE::Actor& actor,
            const RE::PlayerCharacter& player,
            const CommandOptions& options)
        {
            const auto boundary = LiveMovementBoundary(actor, player);
            if (boundary != MovementBoundary::SameArea && !options.confirmed) {
                return std::unexpected(MovementBoundaryWarning(boundary));
            }
            return {};
        }

        bool RefreshConsoleSelectionMovie(RE::Console& console, std::string_view selectionText)
        {
            if (!console.uiMovie ||
                !console.uiMovie->IsAvailable(kConsoleOverlaySelectionMethod.data()) ||
                !console.uiMovie->IsAvailable(kConsoleOverlayClearMethod.data()) ||
                !console.uiMovie->IsAvailable(kMicConsoleRefreshMethod.data())) {
                return false;
            }

            RE::GFxValue argument;
            console.uiMovie->CreateString(&argument, selectionText.data());
            if (!console.uiMovie->Invoke(
                    kConsoleOverlaySelectionMethod.data(),
                    nullptr,
                    std::addressof(argument),
                    1)) {
                return false;
            }
            if (!console.uiMovie->Invoke(
                    kConsoleOverlayClearMethod.data(),
                    nullptr,
                    nullptr,
                    0)) {
                return false;
            }

            RE::GFxValue mode;
            mode.SetNumber(kMicConsoleSelectionMode);
            return console.uiMovie->Invoke(
                kMicConsoleRefreshMethod.data(),
                nullptr,
                std::addressof(mode),
                1);
        }

        ConsoleOverlayReadiness ConsoleOverlayState(const RE::Console& console)
        {
            if (!console.uiMovie ||
                !console.uiMovie->IsAvailable(kConsoleOverlaySelectionMethod.data()) ||
                !console.uiMovie->IsAvailable(kConsoleOverlayClearMethod.data()) ||
                !console.uiMovie->IsAvailable(kMicConsoleRefreshMethod.data())) {
                return ConsoleOverlayReadiness::Unavailable;
            }

            RE::GFxValue destination;
            if (!console.uiMovie->GetVariable(
                    std::addressof(destination),
                    kMicConsoleDestination.data()) ||
                !destination.IsObject() ||
                !destination.HasMember(kMicConsoleDestinationMethod.data())) {
                return ConsoleOverlayReadiness::WaitingForDestination;
            }
            return ConsoleOverlayReadiness::Ready;
        }
    }

    CommandService::CommandService(
        RuntimeIndex& index,
        TrackingService& tracking,
        SavedNpcStore& savedNpcs,
        OperationQueue& operationQueue,
        UiCompletionMailbox& completions,
        const RuntimeSettingsState& settings) noexcept :
        index_(index),
        tracking_(tracking),
        savedNpcs_(savedNpcs),
        operationQueue_(operationQueue),
        completions_(completions),
        settings_(settings)
    {}

    CommandService::~CommandService()
    {
        UnregisterConsoleSelectionEvents();
    }

    void CommandService::RegisterConsoleSelectionEvents()
    {
        if (consoleEventsRegistered_) return;
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
            consoleEventsRegistered_ = true;
        }
    }

    void CommandService::UnregisterConsoleSelectionEvents()
    {
        if (!consoleEventsRegistered_) return;
        if (auto* ui = RE::UI::GetSingleton()) ui->RemoveEventSink<RE::MenuOpenCloseEvent>(this);
        consoleEventsRegistered_ = false;
    }

    void CommandService::CancelPendingOperations() noexcept
    {
        movementGate_.Invalidate();
        enabledStateGate_.Invalidate();
        CancelPendingConsoleSelection();
    }

    void CommandService::CancelPendingConsoleSelection() noexcept
    {
        try {
            std::scoped_lock lock(pendingConsoleMutex_);
            pendingConsoleActive_.store(false, std::memory_order_release);
            pendingConsoleGate_.Cancel();
            pendingConsoleSelection_.reset();
            pendingConsoleRuntimeFormID_ = 0;
            pendingConsoleUpdatePublished_ = false;
            pendingConsoleOverlayRefreshed_ = false;
            pendingConsoleToken_ = {};
            pendingConsoleDeadline_ = {};
        } catch (...) {
            try { logger::error("Could not clear pending console selection state"); }
            catch (...) {}
        }
    }

    bool CommandService::CompletePendingConsoleSelection(std::uint64_t requestSerial) noexcept
    {
        try {
            std::scoped_lock lock(pendingConsoleMutex_);
            if (!pendingConsoleGate_.Complete(requestSerial)) return false;
            pendingConsoleSelection_.reset();
            pendingConsoleRuntimeFormID_ = 0;
            pendingConsoleUpdatePublished_ = false;
            pendingConsoleOverlayRefreshed_ = false;
            pendingConsoleToken_ = {};
            pendingConsoleDeadline_ = {};
            pendingConsoleActive_.store(false, std::memory_order_release);
            return true;
        } catch (...) {
            try { logger::error("Could not complete pending console selection state"); }
            catch (...) {}
            return false;
        }
    }

    bool CommandService::IsEnabledStateRequestCurrent(std::uint64_t requestSerial) const noexcept
    {
        return enabledStateGate_.IsCurrent(requestSerial);
    }

    std::expected<void, std::string> CommandService::Execute(
        CommandKind command,
        const SelectedTarget& target,
        CommandOptions options,
        OperationEpochToken token)
    {
        auto actor = Resolve(target);
        if (!actor) return std::unexpected("NPC is no longer available");
        const auto settings = settings_.Snapshot();

        switch (command) {
        case CommandKind::Travel: return Travel(*actor, options, token);
        case CommandKind::Bring: return Bring(*actor, options, token);
        case CommandKind::Inventory:
            if (!actor->Is3DLoaded()) return std::unexpected("NPC must be loaded to open inventory");
            return OpenInventory(*actor, token);
        case CommandKind::Track: return ToggleTracking(*actor, token);
        case CommandKind::EnableDisable:
        {
            const bool requestedEnabled = options.requestedEnabled.value_or(actor->IsDisabled());
            if (!actor->IsDisabled() &&
                !requestedEnabled &&
                (actor->IsEssential() || actor->IsProtected() || settings->confirmDisable) &&
                !options.confirmed) {
                return std::unexpected("Confirmation is required before disabling this NPC");
            }
            return QueueEnabledStateChange(*actor, requestedEnabled, token);
        }
        case CommandKind::SelectConsole: return SelectInConsole(*actor, token);
        case CommandKind::Favorite: return ToggleFavorite(*actor, target);
        case CommandKind::Count: break;
        }
        return std::unexpected("Unknown command");
    }

    RE::BSEventNotifyControl CommandService::ProcessEvent(
        const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
    {
        return GuardCallback([this, event] {
            if (!event || event->menuName != RE::Console::MENU_NAME) {
                return RE::BSEventNotifyControl::kContinue;
            }
            if (event->opening) RequestPendingConsoleSelectionAttempt();
            else CancelPendingConsoleSelection();
            return RE::BSEventNotifyControl::kContinue;
        }, RE::BSEventNotifyControl::kContinue, [] {
            try { logger::error("Contained exception in the console menu event sink"); }
            catch (...) {}
        });
    }

    RE::NiPointer<RE::Actor> CommandService::Resolve(const SelectedTarget& target) const
    {
        const auto matches = [&](const RE::NiPointer<RE::Actor>& actor) {
            const auto identity = TryGetNpcIdentity(actor.get());
            return actor && !actor->IsPlayerRef() && identity &&
                ResolvedReferenceMatches(target.identity, *identity);
        };
        if (auto actor = target.handle.get(); matches(actor)) return actor;
        if (!target.identity.IsPersistable()) return {};
        if (auto actor = index_.ResolveRuntime(target.ReferenceRuntimeID()); matches(actor)) return actor;
        if (auto actor = index_.Resolve(target.identity.StableReference()); matches(actor)) return actor;
        return {};
    }

    std::expected<void, std::string> CommandService::Travel(
        RE::Actor& actor,
        const CommandOptions& options,
        OperationEpochToken token)
    {
        const auto settings = settings_.Snapshot();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return std::unexpected("Player is unavailable");
        if (actor.IsDead()) return std::unexpected("Dead NPCs are not moved or resurrected");
        if (actor.IsDisabled() && options.disabledMove == DisabledMoveChoice::None) {
            return std::unexpected("Disabled NPC must be enabled before travel");
        }
        if (actor.IsDisabled() && options.disabledMove != DisabledMoveChoice::EnableAndMove) {
            return std::unexpected("Travel to a disabled NPC requires enabling them first");
        }
        if (const auto boundary = RequireBoundaryConfirmation(actor, *player, options); !boundary) return boundary;
        if (settings->confirmTeleport && !options.confirmed) {
            return std::unexpected("Confirmation is required before teleporting");
        }
        return QueueMovement(
            *player,
            actor,
            actor,
            actor.IsDisabled(),
            ShouldSettleAfterLoad(CommandKind::Travel, LiveMovementBoundary(actor, *player)),
            actor.GetFormID(),
            "Travel",
            token);
    }

    std::expected<void, std::string> CommandService::Bring(
        RE::Actor& actor,
        const CommandOptions& options,
        OperationEpochToken token)
    {
        const auto settings = settings_.Snapshot();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return std::unexpected("Player is unavailable");
        if (actor.IsDead()) return std::unexpected("Dead NPCs are not moved or resurrected");
        if (actor.IsDisabled() && options.disabledMove == DisabledMoveChoice::None) {
            return std::unexpected("Disabled NPC must be enabled before moving");
        }
        if (actor.IsDisabled() && options.disabledMove != DisabledMoveChoice::EnableAndMove) {
            return std::unexpected("A disabled NPC must be enabled before moving them");
        }
        if ((actor.IsEssential() || actor.IsProtected()) && !options.confirmed) {
            return std::unexpected("NPC is essential or protected; moving may disrupt quests");
        }
        if (const auto boundary = RequireBoundaryConfirmation(actor, *player, options); !boundary) return boundary;
        if (settings->confirmTeleport && !options.confirmed) {
            return std::unexpected("Confirmation is required before teleporting");
        }
        return QueueMovement(
            actor,
            *player,
            actor,
            actor.IsDisabled(),
            false,
            actor.GetFormID(),
            "Bring",
            token);
    }

    std::expected<void, std::string> CommandService::ToggleTracking(
        RE::Actor& actor, OperationEpochToken token)
    {
        if (tracking_.IsTracked(actor)) return tracking_.Untrack(actor, token);
        const auto tracked = tracking_.Track(actor, token);
        return tracked ? std::expected<void, std::string>{} : std::unexpected(tracked.error());
    }

    std::expected<void, std::string> CommandService::OpenInventory(
        RE::Actor& actor, OperationEpochToken token)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return std::unexpected("The Papyrus virtual machine is unavailable");

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        if (!vm->DispatchStaticCall(
                RE::BSFixedString("WhereaboutsNative"),
                RE::BSFixedString("OpenActorInventory"),
                RE::MakeFunctionArguments(
                    std::addressof(actor), std::int32_t{token.value}),
                callback)) {
            return std::unexpected("Could not queue guarded inventory access");
        }
        return {};
    }

    std::expected<void, std::string> CommandService::ToggleFavorite(
        RE::Actor& actor,
        const SelectedTarget& target)
    {
        auto identity = target.identity.StableReference();
        if (!identity.IsPersistable()) {
            if (const auto stable = TryGetFormIdentity(std::addressof(actor))) identity = *stable;
        }
        if (!identity.IsPersistable()) {
            return std::unexpected("Dynamic NPCs cannot be saved as favorites");
        }

        const auto favorites = savedNpcs_.Favorites();
        const bool exists = std::ranges::any_of(favorites, [&](const auto& entry) {
            return entry.identity == identity;
        });
        const std::string name = actor.GetDisplayFullName() ? actor.GetDisplayFullName() : target.displayName;
        if (exists) {
            static_cast<void>(savedNpcs_.RemoveFavorite(identity));
        } else {
            if (!savedNpcs_.AddFavorite({identity, name})) {
                return std::unexpected("Favorite could not be saved. The list is full or the NPC identity or name is invalid.");
            }
        }
        return {};
    }

    std::expected<void, std::string> CommandService::QueueEnabledStateChange(
        RE::Actor& actor,
        bool expectedEnabled,
        OperationEpochToken token)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return std::unexpected("The Papyrus virtual machine is unavailable");

        const auto requestSerial = enabledStateGate_.Issue();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
            new EnabledStateCompletionCallback(
                actor.GetFormID(),
                expectedEnabled,
                requestSerial,
                token,
                enabledStateGate_,
                operationQueue_,
                completions_)};
        if (!vm->DispatchStaticCall(
                RE::BSFixedString("WhereaboutsNative"),
                RE::BSFixedString("SetActorEnabled"),
                RE::MakeFunctionArguments(
                    std::addressof(actor), bool{expectedEnabled}, std::int32_t{token.value}),
                callback)) {
            return std::unexpected("Could not queue the Papyrus enabled-state change");
        }
        return {};
    }

    std::expected<void, std::string> CommandService::SelectInConsole(
        RE::Actor& actor, OperationEpochToken token)
    {
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!queue) return std::unexpected("The game UI message queue is unavailable");
        {
            std::scoped_lock lock(pendingConsoleMutex_);
            static_cast<void>(pendingConsoleGate_.Start());
            pendingConsoleSelection_ = actor.GetHandle();
            pendingConsoleRuntimeFormID_ = actor.GetFormID();
            pendingConsoleUpdatePublished_ = false;
            pendingConsoleOverlayRefreshed_ = false;
            pendingConsoleToken_ = token;
            pendingConsoleDeadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            pendingConsoleActive_.store(true, std::memory_order_release);
        }
        if (auto* mainWindow = SKSEMenuFramework::GetMainWindow()) mainWindow->IsOpen = false;
        queue->AddMessage(RE::Console::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
        return {};
    }

    void CommandService::TryApplyPendingConsoleSelection(std::uint64_t requestSerial)
    {
        RE::ObjectRefHandle pending;
        std::uint32_t pendingRuntimeFormID = 0;
        bool updatePublished = false;
        bool overlayRefreshed = false;
        bool deadlineExpired = false;
        {
            std::scoped_lock lock(pendingConsoleMutex_);
            if (!pendingConsoleGate_.BeginAttempt(requestSerial)) return;
            pending = pendingConsoleSelection_;
            pendingRuntimeFormID = pendingConsoleRuntimeFormID_;
            updatePublished = pendingConsoleUpdatePublished_;
            overlayRefreshed = pendingConsoleOverlayRefreshed_;
            deadlineExpired = std::chrono::steady_clock::now() >= pendingConsoleDeadline_;
        }
        if (!pending || pendingRuntimeFormID == 0) {
            static_cast<void>(CompletePendingConsoleSelection(requestSerial));
            return;
        }

        auto* ui = RE::UI::GetSingleton();
        auto console = ui ? ui->GetMenu<RE::Console>() : RE::GPtr<RE::Console>{};
        const auto selected = RE::Console::GetSelectedRef();
        const bool verified = selected && selected->GetFormID() == pendingRuntimeFormID;
        const auto overlayReadiness = console && updatePublished && verified ?
            ConsoleOverlayState(*console) :
            ConsoleOverlayReadiness::Unavailable;
        const auto action = DecideConsoleSelectionAction(
            static_cast<bool>(console),
            updatePublished,
            verified,
            overlayReadiness,
            overlayRefreshed,
            deadlineExpired);
        if (action == ConsoleSelectionAction::Complete || action == ConsoleSelectionAction::GiveUp) {
            logger::info(
                "Console selection {}: FormID {:08X}, published {}, verified {}, overlay state {}, overlay refreshed {}",
                action == ConsoleSelectionAction::Complete ? "completed" : "expired",
                pendingRuntimeFormID,
                updatePublished,
                verified,
                static_cast<int>(overlayReadiness),
                overlayRefreshed);
            static_cast<void>(CompletePendingConsoleSelection(requestSerial));
            return;
        }

        if (action == ConsoleSelectionAction::WaitForMenu ||
            action == ConsoleSelectionAction::WaitForOverlay) {
            return;
        }

        if (action == ConsoleSelectionAction::RefreshOverlay) {
            const auto pendingReference = pending.get();
            auto* actor = skyrim_cast<RE::Actor*>(pendingReference.get());
            const auto* actorName = actor ? actor->GetDisplayFullName() : nullptr;
            const auto selectionText = ConsoleSelectionText(
                actorName ? std::string_view{actorName} : std::string_view{},
                pendingRuntimeFormID);
            const bool refreshed = RefreshConsoleSelectionMovie(*console, selectionText);
            {
                std::scoped_lock lock(pendingConsoleMutex_);
                if (pendingConsoleGate_.IsCurrent(requestSerial)) {
                    pendingConsoleOverlayRefreshed_ =
                        pendingConsoleOverlayRefreshed_ || refreshed;
                }
            }
            logger::debug(
                "Optional console overlay refresh: FormID {:08X}, ready true, refreshed {}",
                pendingRuntimeFormID,
                refreshed);
            return;
        }

        console->SetSelectedRef(pending);
        auto* factory = RE::MessageDataFactoryManager::GetSingleton();
        auto* strings = RE::InterfaceStrings::GetSingleton();
        auto* queue = RE::UIMessageQueue::GetSingleton();
        auto* creator = factory && strings ?
            factory->GetCreator<RE::ConsoleData>(strings->consoleData) : nullptr;
        auto* consoleData = creator ? creator->Create() : nullptr;
        bool published = false;
        if (queue && strings && consoleData) {
            consoleData->type = static_cast<RE::ConsoleData::DataType>(1);
            consoleData->pickRef = pending;
            queue->AddMessage(strings->console, RE::UI_MESSAGE_TYPE::kUpdate, consoleData);
            published = true;
        }
        const auto selectedAfterPublish = RE::Console::GetSelectedRef();
        const bool selectionVerifiedAfterPublish =
            selectedAfterPublish && selectedAfterPublish->GetFormID() == pendingRuntimeFormID;
        {
            std::scoped_lock lock(pendingConsoleMutex_);
            if (pendingConsoleGate_.IsCurrent(requestSerial)) {
                pendingConsoleUpdatePublished_ = pendingConsoleUpdatePublished_ ||
                    published || selectionVerifiedAfterPublish;
            }
        }
        logger::debug(
            "Console selection published after menu readiness: FormID {:08X}, update {}, verified {}",
            pendingRuntimeFormID,
            published,
            selectionVerifiedAfterPublish);
        return;
    }

    void CommandService::RequestPendingConsoleSelectionAttempt() noexcept
    {
        if (!pendingConsoleActive_.load(std::memory_order_acquire)) return;

        std::optional<std::uint64_t> requestSerial;
        OperationEpochToken token;
        try {
            std::scoped_lock lock(pendingConsoleMutex_);
            if (!pendingConsoleActive_.load(std::memory_order_acquire)) return;
            requestSerial = pendingConsoleGate_.ClaimQueue();
            token = pendingConsoleToken_;
        } catch (...) {
            CancelPendingConsoleSelection();
            return;
        }
        if (!requestSerial || !token) return;

        const auto submitted = operationQueue_.SubmitUi(token, [this, requestSerial = *requestSerial] {
            TryApplyPendingConsoleSelection(requestSerial);
        });
        if (submitted) return;

        static_cast<void>(CompletePendingConsoleSelection(*requestSerial));
    }

    std::expected<void, std::string> CommandService::QueueMovement(
        RE::Actor& mover,
        RE::TESObjectREFR& destination,
        RE::Actor& selectedActor,
        bool enableSelectedActor,
        bool settleAfterLoad,
        std::uint32_t selectedRuntimeFormID,
        std::string_view command,
        OperationEpochToken token)
    {
        const auto settings = settings_.Snapshot();
        const float distance = std::clamp(settings->teleportRange, 0.0F, 1000.0F);
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return std::unexpected("The Papyrus virtual machine is unavailable");

        const auto requestToken = movementGate_.TryBegin();
        if (!requestToken) {
            logger::warn("Movement request blocked while another relocation is completing: {}", command);
            return std::unexpected("Another movement command is still completing");
        }

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
            new MovementCompletionCallback(
                index_, movementGate_, operationQueue_, token, *requestToken,
                selectedRuntimeFormID, std::string(command))};
        auto* arguments = RE::MakeFunctionArguments(
            std::addressof(mover),
            std::addressof(destination),
            std::addressof(selectedActor),
            bool{enableSelectedActor},
            bool{settleAfterLoad},
            float{distance},
            std::int32_t{token.value});
        logger::info(
            "Movement request: {}, selected FormID {:08X}, mover {:08X}, destination {:08X}, enable {}, settle {}, separation {:.1f}",
            command,
            selectedRuntimeFormID,
            mover.GetFormID(),
            destination.GetFormID(),
            enableSelectedActor,
            settleAfterLoad,
            distance);
        const bool queued = vm->DispatchStaticCall(
            RE::BSFixedString("WhereaboutsNative"),
            RE::BSFixedString("MoveToTarget"),
            arguments,
            callback);
        if (!queued) {
            static_cast<void>(movementGate_.Complete(*requestToken));
            logger::error("Movement dispatch failed: {}, FormID {:08X}", command, selectedRuntimeFormID);
            return std::unexpected("Could not queue safe NPC movement");
        }
        return {};
    }
}
