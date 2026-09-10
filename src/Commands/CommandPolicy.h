#pragma once

#include "Core/NpcSnapshot.h"
#include "Persistence/Settings.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

namespace whereabouts
{
    enum class CommandKind
    {
        Travel,
        Bring,
        Inventory,
        Track,
        EnableDisable,
        SelectConsole,
        Favorite,
        Count
    };

    enum class CommandDecision
    {
        Allowed,
        Confirm,
        Unavailable
    };

    enum class DisabledMoveChoice
    {
        None,
        EnableAndMove
    };

    struct CommandOptions
    {
        bool confirmed{false};
        DisabledMoveChoice disabledMove{DisabledMoveChoice::None};
        std::optional<bool> requestedEnabled;
    };

    [[nodiscard]] bool QueuesPapyrusWork(
        CommandKind command,
        bool targetWasEnabled,
        DisabledMoveChoice disabledMove) noexcept;

    [[nodiscard]] bool ShouldCloseCommandSurface(CommandKind command) noexcept;

    [[nodiscard]] MovementBoundary ClassifyMovementBoundary(
        bool actorHasWorldspace,
        bool playerHasWorldspace,
        bool sameWorldspace,
        bool actorHasCell,
        bool playerHasCell,
        bool sameCell) noexcept;

    [[nodiscard]] const char* MovementBoundaryWarning(MovementBoundary boundary) noexcept;
    [[nodiscard]] bool ShouldSettleAfterLoad(
        CommandKind command, MovementBoundary boundary) noexcept;

    class MovementRequestGate final
    {
    public:
        [[nodiscard]] std::optional<std::uint64_t> TryBegin() noexcept;
        [[nodiscard]] bool Complete(std::uint64_t token) noexcept;
        void Invalidate() noexcept;

    private:
        std::atomic_uint64_t pendingToken_{0};
        std::atomic_uint64_t nextToken_{1};
    };

    struct CommandCheck
    {
        CommandDecision decision{CommandDecision::Unavailable};
        std::string reason;
    };

    class CommandPolicy
    {
    public:
        [[nodiscard]] CommandCheck Check(
            CommandKind command,
            const NpcSnapshot& npc,
            const Settings& settings) const;

        [[nodiscard]] static const char* Label(CommandKind command) noexcept;
    };
}
