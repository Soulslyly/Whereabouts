#pragma once

#include "Commands/CommandPolicy.h"
#include "Lifecycle/RequestSerialGate.h"
#include "Lifecycle/OperationEpoch.h"
#include "Lifecycle/OperationQueue.h"
#include "Lifecycle/UiCompletionMailbox.h"
#include "Persistence/RuntimeSettingsState.h"
#include "Targets/TargetResolver.h"

#include <RE/Skyrim.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <mutex>
#include <string>

namespace whereabouts
{
    class RuntimeIndex;
    class SavedNpcStore;
    class TrackingService;

    class CommandService final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        CommandService(
            RuntimeIndex& index,
            TrackingService& tracking,
            SavedNpcStore& savedNpcs,
            OperationQueue& operationQueue,
            UiCompletionMailbox& completions,
            const RuntimeSettingsState& settings) noexcept;
        ~CommandService() override;

        void RegisterConsoleSelectionEvents();
        void UnregisterConsoleSelectionEvents();
        void CancelPendingOperations() noexcept;
        void RequestPendingConsoleSelectionAttempt() noexcept;
        [[nodiscard]] bool IsEnabledStateRequestCurrent(std::uint64_t requestSerial) const noexcept;

        [[nodiscard]] std::expected<void, std::string> Execute(
            CommandKind command,
            const SelectedTarget& target,
            CommandOptions options,
            OperationEpochToken token);

        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>* source) override;

    private:
        [[nodiscard]] RE::NiPointer<RE::Actor> Resolve(const SelectedTarget& target) const;
        [[nodiscard]] std::expected<void, std::string> Travel(
            RE::Actor& actor, const CommandOptions& options, OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> Bring(
            RE::Actor& actor, const CommandOptions& options, OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> OpenInventory(
            RE::Actor& actor, OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> ToggleTracking(
            RE::Actor& actor, OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> ToggleFavorite(RE::Actor& actor, const SelectedTarget& target);
        [[nodiscard]] std::expected<void, std::string> QueueEnabledStateChange(
            RE::Actor& actor,
            bool expectedEnabled,
            OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> SelectInConsole(
            RE::Actor& actor, OperationEpochToken token);
        void TryApplyPendingConsoleSelection(std::uint64_t requestSerial);
        void CancelPendingConsoleSelection() noexcept;
        [[nodiscard]] bool CompletePendingConsoleSelection(std::uint64_t requestSerial) noexcept;
        [[nodiscard]] std::expected<void, std::string> QueueMovement(
            RE::Actor& mover,
            RE::TESObjectREFR& destination,
            RE::Actor& selectedActor,
            bool enableSelectedActor,
            bool settleAfterLoad,
            std::uint32_t selectedRuntimeFormID,
            std::string_view command,
            OperationEpochToken token);

        RuntimeIndex& index_;
        TrackingService& tracking_;
        SavedNpcStore& savedNpcs_;
        OperationQueue& operationQueue_;
        UiCompletionMailbox& completions_;
        const RuntimeSettingsState& settings_;
        MovementRequestGate movementGate_;
        RequestSerialGate enabledStateGate_;
        mutable std::mutex pendingConsoleMutex_;
        ConsoleAttemptGate pendingConsoleGate_;
        RE::ObjectRefHandle pendingConsoleSelection_;
        std::uint32_t pendingConsoleRuntimeFormID_{0};
        bool pendingConsoleUpdatePublished_{false};
        bool pendingConsoleOverlayRefreshed_{false};
        OperationEpochToken pendingConsoleToken_;
        std::chrono::steady_clock::time_point pendingConsoleDeadline_{};
        std::atomic_bool pendingConsoleActive_{false};
        bool consoleEventsRegistered_{false};
    };
}
