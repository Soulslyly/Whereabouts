#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace whereabouts
{
    inline constexpr std::string_view kConsoleOverlaySelectionMethod =
        "_global.Console.SetCurrentSelection";
    inline constexpr std::string_view kConsoleOverlayClearMethod =
        "_global.Console.ClearExtraInfoArray";
    inline constexpr std::string_view kMicConsoleRefreshMethod =
        "_global.MIC.MICScaleform_GetExtraData";
    inline constexpr std::string_view kMicConsoleDestination =
        "_root.consoleFader_mc.Console_mc";
    inline constexpr std::string_view kMicConsoleDestinationMethod = "AddExtraInfo";
    inline constexpr double kMicConsoleSelectionMode = 0.0;

    class ConsoleTargetSuppression
    {
    public:
        void Suppress(std::uint32_t runtimeFormID) noexcept
        {
            suppressedRuntimeFormID_ = runtimeFormID;
        }

        [[nodiscard]] bool ShouldSuppress(std::uint32_t currentRuntimeFormID) noexcept
        {
            if (suppressedRuntimeFormID_ == 0) return false;
            if (currentRuntimeFormID == suppressedRuntimeFormID_) return true;
            suppressedRuntimeFormID_ = 0;
            return false;
        }

        void Reset() noexcept { suppressedRuntimeFormID_ = 0; }

    private:
        std::uint32_t suppressedRuntimeFormID_{0};
    };

    enum class ConsoleSelectionAction
    {
        WaitForMenu,
        PublishVanilla,
        WaitForOverlay,
        RefreshOverlay,
        Complete,
        GiveUp
    };

    enum class ConsoleOverlayReadiness
    {
        Unavailable,
        WaitingForDestination,
        Ready
    };

    [[nodiscard]] constexpr ConsoleSelectionAction DecideConsoleSelectionAction(
        bool menuReady,
        bool updatePublished,
        bool selectionMatches,
        ConsoleOverlayReadiness overlayReadiness,
        bool overlayRefreshed,
        bool deadlineExpired) noexcept
    {
        if (menuReady && updatePublished && selectionMatches) {
            if (overlayReadiness == ConsoleOverlayReadiness::Unavailable ||
                (overlayReadiness == ConsoleOverlayReadiness::Ready && overlayRefreshed)) {
                return ConsoleSelectionAction::Complete;
            }
        }
        if (deadlineExpired) {
            return ConsoleSelectionAction::GiveUp;
        }
        if (!menuReady) {
            return ConsoleSelectionAction::WaitForMenu;
        }
        if (!updatePublished || !selectionMatches) {
            return ConsoleSelectionAction::PublishVanilla;
        }
        if (overlayReadiness == ConsoleOverlayReadiness::Unavailable) {
            return ConsoleSelectionAction::Complete;
        }
        if (overlayReadiness == ConsoleOverlayReadiness::WaitingForDestination) {
            return ConsoleSelectionAction::WaitForOverlay;
        }
        if (!overlayRefreshed) {
            return ConsoleSelectionAction::RefreshOverlay;
        }
        return ConsoleSelectionAction::Complete;
    }

    class ConsoleAttemptGate
    {
    public:
        [[nodiscard]] std::uint64_t Start() noexcept
        {
            if (serial_ == (std::numeric_limits<std::uint64_t>::max)()) serial_ = 1;
            else if (++serial_ == 0) serial_ = 1;
            pending_ = true;
            queued_ = false;
            return serial_;
        }

        [[nodiscard]] std::optional<std::uint64_t> ClaimQueue() noexcept
        {
            if (!pending_ || queued_) return std::nullopt;
            queued_ = true;
            return serial_;
        }

        [[nodiscard]] bool BeginAttempt(std::uint64_t serial) noexcept
        {
            if (!pending_ || !queued_ || serial != serial_) return false;
            queued_ = false;
            return true;
        }

        [[nodiscard]] bool Complete(std::uint64_t serial) noexcept
        {
            if (!pending_ || serial != serial_) return false;
            pending_ = false;
            queued_ = false;
            return true;
        }

        void Cancel() noexcept
        {
            pending_ = false;
            queued_ = false;
        }

        [[nodiscard]] bool IsPending() const noexcept { return pending_; }
        [[nodiscard]] bool IsCurrent(std::uint64_t serial) const noexcept
        {
            return pending_ && serial == serial_;
        }

    private:
        std::uint64_t serial_{0};
        bool pending_{false};
        bool queued_{false};
    };

    [[nodiscard]] inline std::string ConsoleSelectionText(
        std::string_view displayName,
        std::uint32_t runtimeFormID)
    {
        return std::format(
            "{} [{:08X}]",
            displayName.empty() ? std::string_view{"NPC"} : displayName,
            runtimeFormID);
    }

    [[nodiscard]] constexpr bool IsEligibleNpcTarget(
        bool isActor,
        bool isPlayer,
        bool hasActorBase) noexcept
    {
        return isActor && !isPlayer && hasActorBase;
    }

    enum class TargetSource
    {
        None,
        Console,
        Crosshair,
        Search,
        Tracked,
        Favorite,
        Recent
    };

    [[nodiscard]] constexpr std::string_view TargetSourceLabel(TargetSource source) noexcept
    {
        switch (source) {
        case TargetSource::Console: return "Console";
        case TargetSource::Crosshair: return "Crosshair";
        case TargetSource::Search: return "Search";
        case TargetSource::Tracked: return "Tracked NPCs";
        case TargetSource::Favorite: return "Favorites";
        case TargetSource::Recent: return "Recent";
        case TargetSource::None: return "None";
        }
        return "None";
    }

    [[nodiscard]] constexpr TargetSource ChooseCapturedTarget(
        bool autoSelectConsole,
        bool crosshairFallback,
        bool consoleValid,
        bool crosshairValid) noexcept
    {
        if (autoSelectConsole) {
            if (consoleValid) return TargetSource::Console;
        }
        if (crosshairFallback && crosshairValid) return TargetSource::Crosshair;
        return TargetSource::None;
    }
}
