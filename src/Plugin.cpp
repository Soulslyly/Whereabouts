#include "PCH.h"

#include "API/PapyrusApi.h"
#include "AppContext.h"
#include "Commands/CommandPolicy.h"
#include "Commands/CommandService.h"
#include "Compatibility/MenuFrameworkRuntime.h"
#include "Lifecycle/CallbackGuard.h"
#include "Lifecycle/ProcessContext.h"
#include "Persistence/Serialization.h"
#include "Persistence/SkseSerialization.h"
#include "Persistence/Settings.h"
#include "Search/RuntimeIndex.h"
#include "SKSEMenuFramework.h"
#include "Targets/TargetResolver.h"
#include "Tracking/TrackingService.h"
#include "UI/Menu.h"
#include "UI/Localization.h"
#include "WhereaboutsVersion.h"

namespace
{
    constexpr std::string_view kSettingsPath = "Data/SKSE/Plugins/Whereabouts.ini";
    std::atomic<whereabouts::AppContext*> appContext{nullptr};

    whereabouts::AppContext* Context() noexcept
    {
        return appContext.load(std::memory_order_acquire);
    }

    void LogContainedCallbackException() noexcept
    {
        try { logger::error("Contained exception at a Whereabouts callback boundary"); }
        catch (...) {}
    }

    void __stdcall OnMenuEvent(SKSEMenuFramework::Model::EventType eventType)
    {
        whereabouts::GuardCallbackVoid([eventType] {
            auto* context = Context();
            if (!context) return;
            if (eventType == SKSEMenuFramework::Model::kAfterRender) {
                context->commands.RequestPendingConsoleSelectionAttempt();
                context->menu.OnFrameworkAfterRender();
                return;
            }
            if (eventType == SKSEMenuFramework::Model::kCloseMenu) {
                context->menu.OnFrameworkClosed();
                return;
            }
            if (eventType != SKSEMenuFramework::Model::kOpenMenu) return;
            context->menu.OnFrameworkOpen();
            if (const auto token = context->operationEpoch.Capture()) {
                const auto selectionSerial = context->targets.IssueSelectionRequest();
                static_cast<void>(context->operationQueue.SubmitGame(*token, [
                    token = *token, selectionSerial] {
                    auto* active = Context();
                    if (!active) return;
                    if (!active->targets.IsSelectionRequestCurrent(selectionSerial)) return;
                    static_cast<void>(active->targets.CaptureOnMenuOpen());
                    const auto target = active->targets.Current();
                    if (!active->targets.IsSelectionRequestCurrent(selectionSerial)) return;
                    if (!target) return;
                    static_cast<void>(active->index.RefreshRuntimeId(target->ReferenceRuntimeID()));
                }));
            }
        }, LogContainedCallbackException);
    }

    void SetupLogging()
    {
        const auto logDirectory = SKSE::log::log_directory();
        if (!logDirectory) {
            return;
        }

        const auto logPath = *logDirectory / "Whereabouts.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto log = std::make_shared<spdlog::logger>("Whereabouts", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }

    void OnMessage(SKSE::MessagingInterface::Message* message)
    {
        whereabouts::GuardCallbackVoid([message] {
        auto* context = Context();
        if (!message || !context) return;

        if (message->type == SKSE::MessagingInterface::kNewGame ||
            message->type == SKSE::MessagingInterface::kPostLoadGame) {
            if (message->type == SKSE::MessagingInterface::kNewGame) context->savedNpcs.Clear();
            const auto token = context->BeginSessionBoundary(true);
            if (!context->ResumeSession(token)) {
                logger::warn("Session boundary {} became stale before resume", token.value);
                return;
            }
            logger::info(
                "Operation session {} ready after {}",
                token.value,
                message->type == SKSE::MessagingInterface::kNewGame ? "New Game" : "Post Load");
            if (!context->runtimeReady.load(std::memory_order_acquire)) {
                logger::warn("Runtime capability is unavailable; index rebuild skipped");
                return;
            }
            static_cast<void>(context->indexCoordinator.RebuildNow(
                message->type == SKSE::MessagingInterface::kNewGame ?
                    whereabouts::IndexRequestReason::NewGame :
                    whereabouts::IndexRequestReason::PostLoad));
            return;
        }

        if (message->type != SKSE::MessagingInterface::kDataLoaded) return;

        const auto frameworkProbe = whereabouts::ProbeLoadedMenuFramework();
        const auto frameworkCompatibility = whereabouts::DecideMenuFrameworkCompatibility(
            frameworkProbe.moduleLoaded,
            frameworkProbe.fixedVersion,
            frameworkProbe.requiredExportsAvailable);
        if (frameworkProbe.fixedVersion) {
            const auto version = *frameworkProbe.fixedVersion;
            logger::info(
                "SKSE Menu Framework DLL fixed version {}.{}.{}.{}",
                version.major,
                version.minor,
                version.patch,
                version.build);
        }
        if (frameworkCompatibility != whereabouts::MenuFrameworkCompatibility::Compatible) {
            logger::error(
                "SKSE Menu Framework compatibility rejected: {}{}; "
                "Whereabouts requires DLL fixed version 3.14.x or newer within major version 3",
                whereabouts::MenuFrameworkCompatibilityLabel(frameworkCompatibility),
                frameworkProbe.missingRequiredExport.empty() ?
                    std::string{} :
                    std::format(" ({})", frameworkProbe.missingRequiredExport));
            return;
        }

        logger::info("SKSE Menu Framework compatibility accepted");
        static_cast<void>(whereabouts::ui::InitializeLocalization(
            context->settings.translationLanguage));
        if (!context->serializationReady.load(std::memory_order_acquire) ||
            !context->papyrusReady.load(std::memory_order_acquire)) {
            logger::error("Whereabouts startup capabilities are incomplete; menu registration skipped");
            return;
        }
        const auto initialSession = context->ActivateInitialSession();
        if (!initialSession) {
            logger::warn(
                "Operation session is suspended after a lifecycle boundary; "
                "commands will remain unavailable until that boundary resumes");
        } else {
            logger::info("Operation session {} ready at Data Loaded", initialSession->value);
        }
        bool expected = false;
        if (!context->menuRegistered.compare_exchange_strong(expected, true)) return;
        try {
            context->commands.RegisterConsoleSelectionEvents();
            context->menuEvent.reset(SKSEMenuFramework::AddEvent(OnMenuEvent, 0.0F));
            context->menu.Register();
            context->runtimeReady.store(true, std::memory_order_release);
            logger::info("Whereabouts runtime capability is ready");
        } catch (...) {
            context->runtimeReady.store(false, std::memory_order_release);
            context->menuEvent.reset();
            context->commands.UnregisterConsoleSelectionEvents();
            logger::error(
                "SKSE Menu Framework page registration failed and cannot be rolled back; "
                "Whereabouts will remain disabled for this process and will not retry registration");
        }
        }, LogContainedCallbackException);
    }
}

namespace whereabouts
{
    AppContext* AcquireProcessContext() noexcept
    {
        return appContext.load(std::memory_order_acquire);
    }
}

SKSEPluginVersion = []() constexpr {
    SKSE::PluginVersionData version;
    version.PluginVersion(REL::Version{1, 0, 0, 0});
    version.PluginName("Whereabouts");
    version.AuthorName("Whereabouts");
    version.UsesAddressLibrary();
    version.UsesNoStructs();
    version.versionIndependenceEx |= SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5;
        version.MinimumRequiredXSEVersion(REL::Version{2, 0, 20, 0});
    return version;
}();

SKSE_EXPORT bool SKSEPlugin_Query(SKSE::QueryInterface*, SKSE::PluginInfo* pluginInfo)
{
    return whereabouts::GuardCallback([pluginInfo] {
        if (!pluginInfo) return false;
        pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
        pluginInfo->name = "Whereabouts";
        pluginInfo->version = REL::Version{1, 0, 0, 0}.pack();
        return true;
    }, false, LogContainedCallbackException);
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    return whereabouts::GuardCallback([skse] {
        if (!skse) return false;
        SKSE::Init(skse);
        SetupLogging();

        const auto* tasks = SKSE::GetTaskInterface();
        const auto* serialization = SKSE::GetSerializationInterface();
        const auto* messaging = SKSE::GetMessagingInterface();
        const auto* papyrus = SKSE::GetPapyrusInterface();
        if (!tasks || !serialization || !messaging || !papyrus) {
            logger::critical("Required SKSE interfaces are unavailable");
            return false;
        }

        const whereabouts::SettingsRepository settingsLoader{kSettingsPath};
        auto loadedSettings = settingsLoader.Load();
        auto contextOwner = std::make_unique<whereabouts::AppContext>(
            std::move(loadedSettings.settings),
            kSettingsPath);
        if (contextOwner->settings.debugLogging) spdlog::set_level(spdlog::level::debug);
        for (const auto& warning : loadedSettings.warnings) logger::warn("Settings: {}", warning);

        appContext.store(contextOwner.get(), std::memory_order_release);
        if (!messaging->RegisterListener(OnMessage)) {
            appContext.store(nullptr, std::memory_order_release);
            logger::critical("Could not register the SKSE messaging listener");
            return false;
        }

        auto* context = contextOwner.release();
        if (!whereabouts::RegisterSkseSerialization()) {
            logger::critical(
                "Could not register SKSE serialization callbacks; Whereabouts will remain resident and disabled");
            return true;
        }
        context->serializationReady.store(true, std::memory_order_release);

        if (!papyrus->Register([](RE::BSScript::IVirtualMachine* vm) {
                return whereabouts::GuardCallback([vm] {
                    auto* active = Context();
                    if (!active) return false;
                    const bool ready = whereabouts::RegisterTrackingPapyrus(vm) &&
                        whereabouts::RegisterPublicPapyrus(vm);
                    active->papyrusReady.store(ready, std::memory_order_release);
                    return ready;
                }, false, LogContainedCallbackException);
            })) {
            logger::critical(
                "Could not register the Papyrus callback; Whereabouts will remain resident and disabled");
            return true;
        }

        logger::info(
            "Whereabouts {} initialized for Skyrim {}",
            whereabouts::version::kDisplay,
            REL::Module::get().version().string("."));
        return true;
    }, false, LogContainedCallbackException);
}
