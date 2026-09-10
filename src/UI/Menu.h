#pragma once

#include "UI/Localization.h"

#include "Core/NpcSnapshot.h"
#include "Core/LocationSnapshot.h"
#include "Core/SnapshotState.h"
#include "Core/TrackingCompletion.h"
#include "Commands/CommandPolicy.h"
#include "Commands/CommandService.h"
#include "Lifecycle/OperationEpoch.h"
#include "Lifecycle/OperationQueue.h"
#include "Lifecycle/UiCompletionMailbox.h"
#include "Persistence/Serialization.h"
#include "Persistence/Settings.h"
#include "Persistence/RuntimeSettingsState.h"
#include "Targets/TargetSelection.h"
#include "Search/LocationSearch.h"
#include "Search/SearchEngine.h"
#include "Tracking/UninstallState.h"
#include "UI/MenuModel.h"
#include "UI/LocationTravelGate.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <mutex>
#include <vector>

namespace whereabouts
{
    class RuntimeIndex;
    class IndexCoordinator;
    class TargetResolver;
    class TrackingService;

    namespace ui
    {
        class Menu
        {
        public:
            Menu(
                RuntimeIndex& index,
                IndexCoordinator& indexCoordinator,
                OperationEpoch& operationEpoch,
                OperationQueue& operationQueue,
                UiCompletionMailbox& completions,
                TargetResolver& targets,
                TrackingService& tracking,
                CommandPolicy& commandPolicy,
                CommandService& commandService,
                SavedNpcStore& savedNpcs,
                Settings& settings,
                RuntimeSettingsState& runtimeSettings,
                const SettingsRepository& settingsRepository) noexcept;
            ~Menu();

            void Register();

            void RenderSearch();
            void RenderLocations();
            void RenderTracked();
            void RenderFavorites();
            void RenderRecent();
            void RenderSettings();
            void RenderControllerKeyboard();
            void OnFrameworkOpen();
            void OnFrameworkClosed();
            void OnFrameworkAfterRender();
            void RequestSessionReset() noexcept;

        private:
            static void __stdcall SearchCallback();
            static void __stdcall LocationsCallback();
            static void __stdcall TrackedCallback();
            static void __stdcall FavoritesCallback();
            static void __stdcall RecentCallback();
            static void __stdcall SettingsCallback();

            void RunSearch(SearchRun run, bool reseedRandom = false);
            void RunLocationSearch(SearchRun run, bool mainSearch = false);
            [[nodiscard]] SearchFilters BuildSearchFilters() const;
            void RefreshForIndexGeneration();
            void SyncSelectedTarget();
            void RefreshSelectedSnapshot();
            void QueueIndexRefresh();
            void ApplyTrackingCompletion(TrackingCompletion completion);
            void DrainTrackingCompletions();
            void QueueUseConsoleTarget();
            void QueueUseCrosshairTarget();
            void ClearSelection();
            void SelectSnapshot(const NpcSnapshot& snapshot, TargetSource source);
            void SelectLocation(const LocationSnapshot& snapshot);
            void RenderDetails();
            void RenderLocationDetails();
            void RenderSearchLocationResults();
            void RenderRootModals();
            void RenderCommandConfirmation();
            void RenderTrackingWarning();
            void RenderPrepareForUninstall();
            void RenderLocationTravelConfirmation();
            [[nodiscard]] bool RenderUninstallLockedPage();
            void RenderUninstallLockedSettings();
            void ResumeAfterUninstallPreparation();
            void RequestLocationTravel();
            void ObserveLocationTravelClose(bool frameworkWindowOpen);
            void SubmitPendingLocationTravel();
            void SetLocationStatus(std::string status);
            [[nodiscard]] std::string LocationStatus() const;
            static void CenterNextModal();
            static void DelayedTooltip(const char* text, int flags = 0);
            static void OverflowTooltip(
                const std::string& text,
                float availableWidth,
                int flags = 0);
            static RowInteraction BeginRowInteractionCell(const char* id, float height);
            static void ApplyUnifiedRowBackground(
                const RowInteraction& interaction,
                bool selected);
            static int PushThemeSafeRowColors();
            static void RenderDenseStatusBadges(
                const NpcSnapshot& npc,
                bool missing = false,
                bool selected = false,
                bool favorite = false);
            void RequestCommand(CommandKind command);
            void RequestPrepareForUninstall();
            void ExecuteCommand(CommandKind command, CommandOptions options = {});
            void QueueEnabledStateRefresh(
                OperationEpochToken token,
                std::uint32_t runtimeFormID,
                bool expectedEnabled,
                std::uint64_t requestSerial,
                std::size_t attemptsRemaining);
            [[nodiscard]] bool SubmitGameTask(
                OperationEpochToken token,
                std::function<void(OperationEpochToken)> task);
            void RollbackOptimisticEnabledState(
                const NpcSnapshot& snapshot,
                bool expectedEnabled);
            void SetCommandStatus(std::string status);
            void CloseCommandSurfaces(CommandKind command);
            [[nodiscard]] std::string CommandStatus() const;
            void RenderSavedEntries(const std::vector<SavedNpcEntry>& entries, TargetSource source);
            void SaveSettings();
            void ResetFiltersToDefaults();
            [[nodiscard]] std::optional<NpcSnapshot> FindSnapshot(const FormIdentity& identity) const;
            [[nodiscard]] std::optional<bool> PendingExpectedEnabled(
                std::uint32_t runtimeFormID) const;
            void ClearPendingEnabled(
                std::uint32_t runtimeFormID,
                bool expectedEnabled);

            RuntimeIndex& index_;
            IndexCoordinator& indexCoordinator_;
            OperationEpoch& operationEpoch_;
            OperationQueue& operationQueue_;
            UiCompletionMailbox& completions_;
            TargetResolver& targets_;
            TrackingService& tracking_;
            CommandPolicy& commandPolicy_;
            CommandService& commandService_;
            SavedNpcStore& savedNpcs_;
            Settings& settings_;
            RuntimeSettingsState& runtimeSettings_;
            const SettingsRepository& settingsRepository_;

            std::array<char, 256> searchText_{};
            std::array<char, 256> locationSearchText_{};
            std::array<char, 128> pluginFilter_{};
            std::array<char, 128> locationFilter_{};
            std::vector<std::string> selectedPluginFilters_;
            std::string controllerBackup_;
            std::vector<NpcSnapshot> controllerResultsBackup_;
            std::vector<LocationSnapshot> controllerLocationResultsBackup_;
            std::vector<std::string> controllerSuggestionsBackup_;
            std::size_t controllerResultTotalBackup_{0};
            std::size_t controllerResultTextMatchTotalBackup_{0};
            std::size_t controllerLocationResultTotalBackup_{0};
            std::string controllerSearchErrorBackup_;
            SearchRun controllerSearchRunBackup_{SearchRun::Preview};
            std::string searchError_;
            std::string settingsStatus_;
            std::string indexRefreshStatus_;
            std::string uninstallStatus_;
            std::string savedEntriesStatus_;
            mutable std::mutex commandStatusMutex_;
            mutable std::mutex locationStatusMutex_;
            mutable std::mutex enabledStateMutex_;
            std::string commandStatus_;
            std::string locationStatus_;
            std::vector<NpcSnapshot> results_;
            std::vector<LocationSnapshot> locationResults_;
            std::vector<LocationSnapshot> searchLocationResults_;
            std::vector<std::string> searchSuggestions_;
            std::optional<PendingEnabledState> pendingEnabledState_;
            std::size_t resultTotal_{0};
            std::size_t resultTextMatchTotal_{0};
            std::size_t locationResultTotal_{0};
            std::size_t searchLocationResultTotal_{0};
            SearchRefreshState searchRefreshState_;
            SearchRefreshState locationSearchRefreshState_;
            std::optional<NpcSnapshot> selected_;
            std::optional<LocationSnapshot> selectedLocation_;
            std::optional<LocationSnapshot> pendingLocationTravel_;
            std::optional<CommandKind> pendingCommand_;
            std::uint32_t pendingCommandRuntimeID_{0};
            std::uint64_t seenIndexSession_{static_cast<std::uint64_t>(-1)};
            std::uint64_t seenIndexRevision_{static_cast<std::uint64_t>(-1)};
            std::uint32_t seenTargetFormID_{0};
            TargetSource selectedSource_{TargetSource::None};
            int sortIndex_{0};
            int locationSortIndex_{0};
            std::uint64_t randomSeed_{0};
            int aliveFilter_{0};
            int enabledFilter_{0};
            int teammateFilter_{0};
            int potentialFollowerFilter_{0};
            int loadedFilter_{0};
            bool ascending_{true};
            bool locationAscending_{true};
            bool favoritesOnly_{false};
            bool trackedOnly_{false};
            bool sameLocationOnly_{false};
            SearchContent searchContent_{SearchContent::NpcsOnly};
            ResultSectionOrder resultSectionOrder_{ResultSectionOrder::NpcsFirst};
            bool includeGeneric_{false};
            bool genericOnly_{false};
            bool genericAutoContext_{false};
            bool pluginSuggestionsDismissed_{false};
            bool searchAfterIndexRefresh_{false};
            bool searchSortUiDirty_{false};
            float searchPaneRatio_{0.46F};
            float locationPaneRatio_{0.55F};
            bool openCommandConfirmation_{false};
            bool openTrackingWarning_{false};
            bool openPrepareForUninstall_{false};
            bool openLocationTravelConfirmation_{false};
            bool openControllerKeyboard_{false};
            bool locationTravelArmed_{false};
            LocationTravelGate locationTravelGate_;
            std::uint64_t locationTravelGeneration_{0};
            bool commandsBlocked_{false};
            ManualRefreshState manualRefreshState_{ManualRefreshState::Idle};
            std::uint64_t manualRefreshSession_{0};
            std::uint64_t manualRefreshRevision_{0};
            UninstallPhase uninstallPhase_{UninstallPhase::Idle};
            std::atomic_bool sessionResetRequested_{false};
            std::atomic_bool frameworkOpen_{false};
        };
    }
}
