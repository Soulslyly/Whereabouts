#pragma once

#include "Commands/CommandPolicy.h"
#include "Lifecycle/OperationEpoch.h"

#include <cstdint>
#include <array>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace whereabouts
{
    struct CommandActionDefinition
    {
        std::string_view id;
        CommandKind command;
    };

    inline constexpr std::array kBuiltInCommandActions{
        CommandActionDefinition{"travel", CommandKind::Travel},
        CommandActionDefinition{"bring", CommandKind::Bring},
        CommandActionDefinition{"inventory", CommandKind::Inventory},
        CommandActionDefinition{"track", CommandKind::Track},
        CommandActionDefinition{"enable_disable", CommandKind::EnableDisable},
        CommandActionDefinition{"select_console", CommandKind::SelectConsole},
        CommandActionDefinition{"favorite", CommandKind::Favorite},
        CommandActionDefinition{"stop_combat", CommandKind::StopCombat},
        CommandActionDefinition{"make_essential", CommandKind::MakeEssential},
        CommandActionDefinition{"make_protected", CommandKind::MakeProtected},
        CommandActionDefinition{"remove_flags", CommandKind::RemoveFlags},
        CommandActionDefinition{"restore_original_flags", CommandKind::RestoreOriginalFlags}};

    inline constexpr std::string_view kCopyNpcReportActionID = "copy_report";
    inline constexpr std::string_view kActorFlagsGroupActionID = "actor_flags";
    inline constexpr std::string_view kCopyIdentityActionID = "copy_id";

    [[nodiscard]] constexpr bool IsActorFlagActionID(std::string_view actionID) noexcept
    {
        return actionID == "make_essential" || actionID == "make_protected" ||
            actionID == "remove_flags" || actionID == "restore_original_flags";
    }

    [[nodiscard]] constexpr std::optional<CommandKind> CommandForActionID(
        std::string_view actionID) noexcept
    {
        for (const auto& action : kBuiltInCommandActions) {
            if (action.id == actionID) return action.command;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> CustomSlotForActionID(
        std::string_view actionID) noexcept;
    [[nodiscard]] std::string CustomActionID(std::size_t slot);

    enum class SafetyFlagOperation
    {
        MakeEssential,
        MakeProtected,
        RemoveFlags,
        RestoreOriginal
    };

    struct ActorFlagPair
    {
        bool essential{false};
        bool protectedActor{false};

        [[nodiscard]] bool operator==(const ActorFlagPair&) const noexcept = default;
    };

    [[nodiscard]] constexpr ActorFlagPair ApplySafetyFlagOperation(
        ActorFlagPair current,
        SafetyFlagOperation operation) noexcept
    {
        switch (operation) {
        case SafetyFlagOperation::MakeEssential:
            current.essential = true;
            break;
        case SafetyFlagOperation::MakeProtected:
            current.protectedActor = true;
            break;
        case SafetyFlagOperation::RemoveFlags:
            current = {};
            break;
        case SafetyFlagOperation::RestoreOriginal:
            break;
        }
        return current;
    }

    [[nodiscard]] std::expected<void, std::string> ApplyVerifiedActorFlagMutation(
        ActorFlagPair before,
        ActorFlagPair desired,
        const std::function<void(ActorFlagPair)>& write,
        const std::function<ActorFlagPair()>& read);

    class OriginalActorFlagStore
    {
    public:
        void BeginSession(OperationEpochToken token) noexcept;
        void Clear() noexcept;
        void RememberAfterSuccessfulChange(
            std::uint32_t ownerRuntimeFormID,
            ActorFlagPair original,
            OperationEpochToken token) noexcept;
        [[nodiscard]] std::optional<ActorFlagPair> Original(
            std::uint32_t ownerRuntimeFormID,
            OperationEpochToken token) const noexcept;

    private:
        mutable std::mutex mutex_;
        OperationEpochToken session_;
        std::unordered_map<std::uint32_t, ActorFlagPair> originals_;
    };

    [[nodiscard]] std::expected<void, std::string> ValidateCustomCommand(
        std::string_view label,
        std::string_view command);
    [[nodiscard]] std::expected<std::string, std::string> ExpandCustomCommand(
        std::string_view command,
        std::uint32_t referenceRuntimeFormID,
        std::uint32_t baseRuntimeFormID);

    enum class CommandConfirmationRoute
    {
        ExecuteNow,
        TrackingWarning,
        CommandConfirmation,
        Unavailable
    };

    [[nodiscard]] constexpr CommandConfirmationRoute ConfirmationRoute(
        bool confirmationsEnabled,
        CommandDecision decision,
        bool needsTrackingWarning) noexcept
    {
        if (decision == CommandDecision::Unavailable) {
            return CommandConfirmationRoute::Unavailable;
        }
        if (!confirmationsEnabled) return CommandConfirmationRoute::ExecuteNow;
        if (needsTrackingWarning) return CommandConfirmationRoute::TrackingWarning;
        if (decision == CommandDecision::Confirm) {
            return CommandConfirmationRoute::CommandConfirmation;
        }
        return CommandConfirmationRoute::ExecuteNow;
    }

    [[nodiscard]] std::string SharedBaseWarning(std::size_t indexedReferenceCount);
    [[nodiscard]] std::string FormatNpcReport(const NpcSnapshot& npc);

    enum class RowActivation
    {
        SingleClick,
        RowButton,
        DoubleClick
    };

    [[nodiscard]] constexpr std::string_view QuickActionForActivation(
        std::string_view rowButtonAction,
        std::string_view doubleClickAction,
        RowActivation activation) noexcept
    {
        if (activation == RowActivation::RowButton) return rowButtonAction;
        if (activation == RowActivation::DoubleClick) return doubleClickAction;
        return {};
    }
}
