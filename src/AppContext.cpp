#include "PCH.h"

#include "AppContext.h"
#include "Lifecycle/OperationQueueSkyrim.h"
#include "SKSEMenuFramework.h"

namespace whereabouts
{
    AppContext::AppContext(Settings loadedSettings, std::filesystem::path settingsPath) :
        operationQueue(operationEpoch, MakeGameTaskSubmitter(), MakeUiTaskSubmitter(), [] {
            try { logger::error("Contained exception in an asynchronous Whereabouts task"); }
            catch (...) {}
        }),
        settings(std::move(loadedSettings)),
        runtimeSettings(settings),
        settingsRepository(std::move(settingsPath)),
        tracking(savedNpcs, operationEpoch, operationQueue, uiCompletions),
        indexCoordinator(index, tracking),
        targets(index, runtimeSettings),
        commands(index, tracking, savedNpcs, operationQueue, uiCompletions, runtimeSettings),
        menu(index, indexCoordinator, operationEpoch, operationQueue, uiCompletions, targets, tracking, commandPolicy, commands, savedNpcs, settings, runtimeSettings, settingsRepository)
    {
        savedNpcs.SetRecentLimit(settings.recentLimit);
    }

    AppContext::~AppContext() = default;

    std::optional<OperationEpochToken> AppContext::ActivateInitialSession() noexcept
    {
        return operationEpoch.ActivateInitial();
    }

    OperationEpochToken AppContext::BeginSessionBoundary(bool publishIndexView) noexcept
    {
        const auto token = operationEpoch.SuspendAndAdvance();
        uiCompletions.Clear();
        commands.CancelPendingOperations();
        tracking.CancelPendingOperations();
        targets.Clear();
        menu.RequestSessionReset();
        indexCoordinator.BeginSessionBoundary(publishIndexView);
        return token;
    }

    bool AppContext::ResumeSession(OperationEpochToken token) noexcept
    {
        return operationEpoch.Resume(token);
    }

}
