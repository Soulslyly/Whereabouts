#pragma once

namespace whereabouts
{
    enum class UninstallPhase
    {
        Idle,
        ClearingQuest,
        Complete,
        Failed,
        Resuming,
        ResumeFailed
    };

    enum class UninstallEvent
    {
        Confirm,
        QuestCleared,
        Failure,
        Resume,
        ResumeReady,
        ResumeFailed
    };

    [[nodiscard]] constexpr UninstallPhase NextUninstallPhase(
        UninstallPhase phase,
        UninstallEvent event) noexcept
    {
        if ((phase == UninstallPhase::Complete || phase == UninstallPhase::ResumeFailed) && event == UninstallEvent::Resume) {
            return UninstallPhase::Resuming;
        }
        if (phase == UninstallPhase::Resuming) {
            if (event == UninstallEvent::ResumeReady) return UninstallPhase::Idle;
            if (event == UninstallEvent::ResumeFailed) return UninstallPhase::ResumeFailed;
        }
        if ((phase == UninstallPhase::Idle || phase == UninstallPhase::Failed) &&
            event == UninstallEvent::Confirm) {
            return UninstallPhase::ClearingQuest;
        }
        if (phase == UninstallPhase::ClearingQuest) {
            if (event == UninstallEvent::QuestCleared) return UninstallPhase::Complete;
            if (event == UninstallEvent::Failure) return UninstallPhase::Failed;
        }
        return phase;
    }

    [[nodiscard]] constexpr bool IsUninstallLocked(UninstallPhase phase) noexcept
    {
        return phase == UninstallPhase::Complete || phase == UninstallPhase::Resuming ||
            phase == UninstallPhase::ResumeFailed;
    }
}
