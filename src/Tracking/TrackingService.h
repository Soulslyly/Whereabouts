#pragma once

#include "Core/TrackingCompletion.h"
#include "Lifecycle/OperationEpoch.h"
#include "Lifecycle/OperationQueue.h"
#include "Lifecycle/UiCompletionMailbox.h"
#include "Tracking/TrackingOperationLedger.h"
#include "Tracking/TrackingQuestLayout.h"
#include "Tracking/TrackingLifecycle.h"

#include <RE/Skyrim.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace whereabouts
{
    class SavedNpcStore;

    class TrackingService
    {
    public:
        TrackingService(
            SavedNpcStore& savedNpcs,
            OperationEpoch& operationEpoch,
            OperationQueue& operationQueue,
            UiCompletionMailbox& completions) noexcept;

        [[nodiscard]] bool IsTracked(RE::Actor& actor) const;
        [[nodiscard]] bool Busy() const;
        [[nodiscard]] std::expected<std::uint32_t, std::string> Track(
            RE::Actor& actor, OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> Untrack(
            RE::Actor& actor, OperationEpochToken token);
        [[nodiscard]] std::vector<RE::ObjectRefHandle> ActiveAliases() const;
        [[nodiscard]] std::expected<void, std::string> RefreshMarkers();
        [[nodiscard]] std::expected<void, std::string> RefreshMarkers(OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> ClearAll(OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> PrepareForUninstall(OperationEpochToken token);
        [[nodiscard]] std::expected<void, std::string> Resume(OperationEpochToken token);
        void SetUninstallLocked(bool locked) noexcept;
        void CancelPendingOperations() noexcept;
        void HandleTrackedDeath(RE::Actor* actor, std::int32_t objectiveIndex);
        void HandleTrackedDeathRemoved(RE::Actor* actor);
        void CompletePapyrusDispatch(
            TrackingCompletion completion,
            bool expectsBoolean,
            RE::BSScript::Variable result);
    private:
        [[nodiscard]] RE::TESQuest* ResolveQuest() const;
        [[nodiscard]] std::expected<
            std::array<RE::BGSRefAlias*, kTrackingSlotCount>, std::string>
            InspectLayout(RE::TESQuest& quest) const;
        [[nodiscard]] bool IsQuestScriptBound(RE::TESQuest& quest) const;
        [[nodiscard]] bool EnsureQuestStarted(
            RE::TESQuest& quest,
            TrackingLifecycleEvent event,
            const TrackingOperationTicket& ticket,
            std::uint32_t runtimeFormID);
        void QueueActorDispatch(
            RE::ObjectRefHandle actor,
            std::string method,
            TrackingOperationTicket ticket,
            std::uint32_t runtimeFormID,
            std::size_t attemptsRemaining);
        void QueueNoArgumentDispatch(
            std::string method,
            TrackingOperationTicket ticket,
            std::size_t attemptsRemaining);
        void QueueObjectiveDispatch(
            RE::ObjectRefHandle expectedReference,
            TrackingOperationTicket ticket,
            std::uint32_t runtimeFormID,
            std::size_t attemptsRemaining);
        void QueueEmptyObjectiveDispatch(
            TrackingOperationTicket ticket,
            std::uint32_t runtimeFormID,
            std::size_t attemptsRemaining);
        void ReconcileTrackedDeaths(OperationEpochToken token);
        void RememberTrackedDeath(RE::Actor& actor, std::uint16_t objectiveIndex);
        void MarkTrackedDeathMissing(std::uint32_t runtimeFormID);
        void RemoveActiveTrackedDeaths();
        void HandleCompletion(TrackingCompletion completion);
        void QueueStopAndReset(
            TrackingOperationTicket ticket,
            std::size_t attemptsRemaining,
            std::uint8_t phase = 0,
            std::optional<TrackingCompletion> completion = std::nullopt);
        void ReportFailure(
            const TrackingOperationTicket& ticket,
            std::uint32_t runtimeFormID,
            std::string message);
        void PublishCompletion(TrackingCompletion completion);
        void DrainDeferred(OperationEpochToken token);
        [[nodiscard]] std::vector<std::uint32_t> ActiveRuntimeIds() const;
        [[nodiscard]] bool DispatchActorMethod(
            RE::TESQuest& quest,
            std::string_view method,
            RE::Actor& actor,
            const TrackingOperationTicket& ticket,
            std::uint32_t runtimeFormID);
        [[nodiscard]] bool DispatchNoArgumentMethod(
            RE::TESQuest& quest,
            std::string_view method,
            const TrackingOperationTicket& ticket);
        [[nodiscard]] bool DispatchObjectiveMethod(
            RE::TESQuest& quest,
            RE::TESObjectREFR& expectedReference,
            const TrackingOperationTicket& ticket,
            std::uint32_t runtimeFormID);
        [[nodiscard]] bool DispatchEmptyObjectiveMethod(
            RE::TESQuest& quest,
            const TrackingOperationTicket& ticket,
            std::uint32_t runtimeFormID);

        SavedNpcStore& savedNpcs_;
        OperationEpoch& operationEpoch_;
        OperationQueue& operationQueue_;
        UiCompletionMailbox& completions_;
        TrackingOperationLedger ledger_;
        std::atomic_bool uninstallLocked_{false};
    };

    bool RegisterTrackingPapyrus(RE::BSScript::IVirtualMachine* vm);
}
