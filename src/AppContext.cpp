#include "PCH.h"

#include "AppContext.h"
#include "Lifecycle/OperationQueueSkyrim.h"
#include "SKSEMenuFramework.h"

namespace whereabouts
{
    AppContext::AppContext(
        Settings loadedSettings,
        std::filesystem::path settingsPath,
        std::filesystem::path sharedFavoritesPath) :
        operationQueue(operationEpoch, MakeGameTaskSubmitter(), MakeUiTaskSubmitter(), [] {
            try { logger::error("Contained exception in an asynchronous Whereabouts task"); }
            catch (...) {}
        }),
        settings(std::move(loadedSettings)),
        runtimeSettings(settings),
        settingsRepository(std::move(settingsPath)),
        sharedFavoritesRepository(std::move(sharedFavoritesPath)),
        favorites(savedNpcs, sharedFavoritesRepository),
        tracking(savedNpcs, operationEpoch, operationQueue, uiCompletions),
        indexCoordinator(index, tracking),
        targets(index, runtimeSettings),
        commands(index, tracking, favorites, operationQueue, uiCompletions, runtimeSettings),
        menu(index, indexCoordinator, operationEpoch, operationQueue, uiCompletions, targets, tracking, commandPolicy, commands, savedNpcs, favorites, settings, runtimeSettings, settingsRepository)
    {
        savedNpcs.SetRecentLimit(settings.recentLimit);
        if (const auto enabled = favorites.SetSharingEnabled(settings.shareFavoritesAcrossSaves);
            !enabled) {
            settings.shareFavoritesAcrossSaves = false;
            runtimeSettings.Publish(settings);
            logger::error("Shared Favorites could not be enabled: {}", enabled.error());
        }
    }

    AppContext::~AppContext() = default;

    std::optional<OperationEpochToken> AppContext::ActivateInitialSession() noexcept
    {
        const auto token = operationEpoch.ActivateInitial();
        if (token) commands.BeginSession(*token);
        return token;
    }

    OperationEpochToken AppContext::BeginSessionBoundary(bool publishIndexView) noexcept
    {
        const auto token = operationEpoch.SuspendAndAdvance();
        uiCompletions.Clear();
        commands.CancelPendingOperations();
        commands.ClearTransientState();
        tracking.CancelPendingOperations();
        targets.Clear();
        menu.RequestSessionReset();
        indexCoordinator.BeginSessionBoundary(publishIndexView);
        return token;
    }

    bool AppContext::ResumeSession(OperationEpochToken token) noexcept
    {
        const bool resumed = operationEpoch.Resume(token);
        if (resumed) commands.BeginSession(token);
        return resumed;
    }

}
