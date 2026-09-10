#pragma once

#include "Commands/CommandPolicy.h"
#include "Commands/CommandService.h"
#include "Lifecycle/OperationEpoch.h"
#include "Lifecycle/OperationQueue.h"
#include "Lifecycle/UiCompletionMailbox.h"
#include "Persistence/Serialization.h"
#include "Persistence/Settings.h"
#include "Persistence/RuntimeSettingsState.h"
#include "Search/RuntimeIndex.h"
#include "Search/IndexCoordinator.h"
#include "Targets/TargetResolver.h"
#include "Tracking/TrackingService.h"
#include "UI/Menu.h"

#include <memory>
#include <atomic>

namespace SKSEMenuFramework::Model
{
    class Event;
}

namespace whereabouts
{
    class AppContext
    {
    public:
        AppContext(Settings loadedSettings, std::filesystem::path settingsPath);
        ~AppContext();

        [[nodiscard]] std::optional<OperationEpochToken> ActivateInitialSession() noexcept;
        [[nodiscard]] OperationEpochToken BeginSessionBoundary(
            bool publishIndexView = false) noexcept;
        [[nodiscard]] bool ResumeSession(OperationEpochToken token) noexcept;

        OperationEpoch operationEpoch;
        OperationQueue operationQueue;
        UiCompletionMailbox uiCompletions;
        Settings settings;
        RuntimeSettingsState runtimeSettings;
        SettingsRepository settingsRepository;
        SavedNpcStore savedNpcs;
        TrackingService tracking;
        RuntimeIndex index;
        IndexCoordinator indexCoordinator;
        TargetResolver targets;
        CommandPolicy commandPolicy;
        CommandService commands;
        ui::Menu menu;
        std::unique_ptr<SKSEMenuFramework::Model::Event> menuEvent;
        std::atomic_bool serializationReady{false};
        std::atomic_bool papyrusReady{false};
        std::atomic_bool menuRegistered{false};
        std::atomic_bool runtimeReady{false};
    };
}
