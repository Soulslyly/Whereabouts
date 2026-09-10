#include "Commands/CommandPolicy.h"

namespace whereabouts
{
    MovementBoundary ClassifyMovementBoundary(
        bool actorHasWorldspace,
        bool playerHasWorldspace,
        bool sameWorldspace,
        bool actorHasCell,
        bool playerHasCell,
        bool sameCell) noexcept
    {
        if (actorHasWorldspace != playerHasWorldspace) {
            return MovementBoundary::DifferentWorldspace;
        }
        if (actorHasWorldspace) {
            return sameWorldspace ? MovementBoundary::SameArea : MovementBoundary::DifferentWorldspace;
        }
        if (!actorHasCell || !playerHasCell) return MovementBoundary::Unknown;
        return sameCell ? MovementBoundary::SameArea : MovementBoundary::DifferentCell;
    }

    const char* MovementBoundaryWarning(MovementBoundary boundary) noexcept
    {
        switch (boundary) {
        case MovementBoundary::DifferentCell:
            return "NPC is in a different interior cell; confirm teleport";
        case MovementBoundary::DifferentWorldspace:
            return "NPC is in a different worldspace; confirm teleport";
        case MovementBoundary::Unknown:
            return "NPC location relationship is unknown; confirm teleport";
        case MovementBoundary::SameArea:
            break;
        }
        return "";
    }

    bool ShouldSettleAfterLoad(CommandKind command, MovementBoundary boundary) noexcept
    {
        return command == CommandKind::Travel && boundary != MovementBoundary::SameArea;
    }

    std::optional<std::uint64_t> MovementRequestGate::TryBegin() noexcept
    {
        const auto token = nextToken_.fetch_add(1, std::memory_order_relaxed);
        std::uint64_t available = 0;
        if (!pendingToken_.compare_exchange_strong(
                available,
                token,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return std::nullopt;
        }
        return token;
    }

    bool MovementRequestGate::Complete(std::uint64_t token) noexcept
    {
        if (token == 0) return false;
        return pendingToken_.compare_exchange_strong(
            token,
            0,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    void MovementRequestGate::Invalidate() noexcept
    {
        pendingToken_.store(0, std::memory_order_release);
    }

    bool QueuesPapyrusWork(
        CommandKind command,
        bool,
        DisabledMoveChoice) noexcept
    {
        switch (command) {
        case CommandKind::Inventory:
        case CommandKind::Track:
        case CommandKind::EnableDisable:
        case CommandKind::Travel:
        case CommandKind::Bring:
            return true;
        case CommandKind::SelectConsole:
        case CommandKind::Favorite:
        case CommandKind::Count:
            return false;
        }
        return false;
    }

    bool ShouldCloseCommandSurface(CommandKind command) noexcept
    {
        switch (command) {
        case CommandKind::Travel:
        case CommandKind::Inventory:
        case CommandKind::SelectConsole:
            return true;
        case CommandKind::Bring:
        case CommandKind::Track:
        case CommandKind::EnableDisable:
        case CommandKind::Favorite:
        case CommandKind::Count:
            return false;
        }
        return false;
    }

    CommandCheck CommandPolicy::Check(
        CommandKind command,
        const NpcSnapshot& npc,
        const Settings& settings) const
    {
        if (!npc.available || npc.ReferenceRuntimeID() == 0) {
            return {CommandDecision::Unavailable, "NPC is no longer available"};
        }

        switch (command) {
        case CommandKind::Travel:
        case CommandKind::Bring:
            if (!npc.alive) {
                return {CommandDecision::Unavailable, "Dead NPCs are not moved or resurrected"};
            }
            if (!npc.enabled) {
                return {CommandDecision::Confirm, "NPC is disabled; confirm enabling before moving"};
            }
            if (command == CommandKind::Bring && (npc.essential || npc.protectedActor)) {
                return {CommandDecision::Confirm, "NPC is essential or protected; moving may disrupt quests"};
            }
            if (npc.spatial.movementBoundary != MovementBoundary::SameArea) {
                return {CommandDecision::Confirm, MovementBoundaryWarning(npc.spatial.movementBoundary)};
            }
            if (settings.confirmTeleport) {
                return {CommandDecision::Confirm, "Confirm teleport"};
            }
            return {CommandDecision::Allowed, {}};

        case CommandKind::Inventory:
            if (!npc.loaded) {
                return {CommandDecision::Unavailable, "NPC must be loaded to open inventory"};
            }
            return {CommandDecision::Allowed, {}};

        case CommandKind::Track:
            if (npc.trackingFull && !npc.tracked) {
                return {CommandDecision::Unavailable, "All 100 tracking slots are in use"};
            }
            return {CommandDecision::Allowed, {}};

        case CommandKind::EnableDisable:
            if (npc.enabled && (npc.essential || npc.protectedActor)) {
                return {CommandDecision::Confirm, "NPC is essential or protected; disabling may disrupt quests"};
            }
            if (npc.enabled && settings.confirmDisable) {
                return {CommandDecision::Confirm, "Confirm disabling this NPC"};
            }
            return {CommandDecision::Allowed, {}};

        case CommandKind::Favorite:
            if (!npc.StableReference().IsPersistable()) {
                return {CommandDecision::Unavailable, "Dynamic NPCs cannot be saved as favorites"};
            }
            return {CommandDecision::Allowed, {}};

        case CommandKind::SelectConsole:
            return {CommandDecision::Allowed, {}};
        case CommandKind::Count:
            break;
        }
        return {CommandDecision::Unavailable, "Unknown command"};
    }

    const char* CommandPolicy::Label(CommandKind command) noexcept
    {
        switch (command) {
        case CommandKind::Travel: return "Travel";
        case CommandKind::Bring: return "Bring";
        case CommandKind::Inventory: return "Inventory";
        case CommandKind::Track: return "Track";
        case CommandKind::EnableDisable: return "Enable / Disable";
        case CommandKind::SelectConsole: return "Select in Console";
        case CommandKind::Favorite: return "Favorite";
        case CommandKind::Count: break;
        }
        return "Command";
    }
}
