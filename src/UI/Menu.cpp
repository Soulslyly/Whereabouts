#include "PCH.h"

#include "AppContext.h"
#include "Core/SpatialPresentation.h"

#include "Commands/CommandPolicy.h"
#include "Commands/CommandService.h"
#include "Core/DistanceFormat.h"
#include "Core/FormIdentityAdapter.h"
#include "Core/SnapshotState.h"
#include "Lifecycle/CallbackGuard.h"
#include "Lifecycle/ProcessContext.h"
#include "Search/RuntimeIndex.h"
#include "Search/LocationSearch.h"
#include "Search/IndexCoordinator.h"
#include "SKSEMenuFramework.h"
#include "Targets/TargetResolver.h"
#include "Tracking/TrackingService.h"
#include "UI/Menu.h"
#include "UI/MenuModel.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <string_view>

namespace whereabouts::ui
{
    namespace
    {
        void LogMenuCallbackException() noexcept
        {
            try { logger::error("Contained exception in an SKSE Menu Framework page callback"); }
            catch (...) {}
        }

        float ColorDistance(const ImGuiMCP::ImVec4& left, const ImGuiMCP::ImVec4& right) noexcept
        {
            return std::abs(left.x - right.x) + std::abs(left.y - right.y) +
                std::abs(left.z - right.z);
        }

        std::string LocalizedCopyConfirmation(const CopyIdentitySelection& selection) noexcept
        {
            if (selection.value.empty()) return TranslateOwned("No ID is available for this NPC.");
            const auto requested = TranslateOwned(CopyIdentityLabel(selection.requested));
            const auto copied = TranslateOwned(CopyIdentityLabel(selection.copied));
            return selection.usedFallback ?
                TranslateFormat("No {} - copied {} {}.", requested, copied, selection.value) :
                TranslateFormat("Copied {} {}.", copied, selection.value);
        }

        std::string ActionLabel(const Settings& settings, std::string_view actionID)
        {
            if (actionID == kDisabledActionID) return "Off";
            if (actionID == kCopyNpcReportActionID) return "Copy NPC Report";
            if (const auto command = CommandForActionID(actionID)) {
                return CommandPolicy::Label(*command);
            }
            if (const auto slot = CustomSlotForActionID(actionID)) {
                const auto& custom = settings.customCommands[*slot];
                return custom.name.empty() ?
                    std::format("Custom Command {}", *slot + 1) : custom.name;
            }
            return "Unavailable action";
        }

    }

    Menu::Menu(
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
        FavoriteService& favorites,
        Settings& settings,
        RuntimeSettingsState& runtimeSettings,
        const SettingsRepository& settingsRepository) noexcept :
        index_(index),
        indexCoordinator_(indexCoordinator),
        operationEpoch_(operationEpoch),
        operationQueue_(operationQueue),
        completions_(completions),
        targets_(targets),
        tracking_(tracking),
        commandPolicy_(commandPolicy),
        commandService_(commandService),
        savedNpcs_(savedNpcs),
        favorites_(favorites),
        settings_(settings),
        runtimeSettings_(runtimeSettings),
        settingsRepository_(settingsRepository),
        randomSeed_(static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()))
    {}

    Menu::~Menu()
    {}

    void Menu::Register()
    {
        sortIndex_ = static_cast<int>(SortKey::Name);
        SKSEMenuFramework::SetSection(TranslateText("Whereabouts"));
        SKSEMenuFramework::AddSectionItem(TranslateText(kPageNames[0]), SearchCallback);
        SKSEMenuFramework::AddSectionItem(TranslateText(kPageNames[1]), LocationsCallback);
        SKSEMenuFramework::AddSectionItem(TranslateText(kPageNames[2]), TrackedCallback);
        SKSEMenuFramework::AddSectionItem(TranslateText(kPageNames[3]), FavoritesCallback);
        SKSEMenuFramework::AddSectionItem(TranslateText(kPageNames[4]), RecentCallback);
        SKSEMenuFramework::AddSectionItem(TranslateText(kPageNames[5]), InspectorCallback);
        SKSEMenuFramework::AddSectionItem(TranslateText(kPageNames[6]), SettingsCallback);
    }
    void __stdcall Menu::LocationsCallback()
    {
        GuardCallbackVoid([] {
            if (auto* context = AcquireProcessContext()) context->menu.RenderLocations();
        }, LogMenuCallbackException);
    }

    void __stdcall Menu::SearchCallback()
    {
        GuardCallbackVoid([] {
            if (auto* context = AcquireProcessContext()) context->menu.RenderSearch();
        }, LogMenuCallbackException);
    }
    void __stdcall Menu::TrackedCallback()
    {
        GuardCallbackVoid([] {
            if (auto* context = AcquireProcessContext()) context->menu.RenderTracked();
        }, LogMenuCallbackException);
    }
    void __stdcall Menu::FavoritesCallback()
    {
        GuardCallbackVoid([] {
            if (auto* context = AcquireProcessContext()) context->menu.RenderFavorites();
        }, LogMenuCallbackException);
    }
    void __stdcall Menu::RecentCallback()
    {
        GuardCallbackVoid([] {
            if (auto* context = AcquireProcessContext()) context->menu.RenderRecent();
        }, LogMenuCallbackException);
    }
    void __stdcall Menu::InspectorCallback()
    {
        GuardCallbackVoid([] {
            if (auto* context = AcquireProcessContext()) context->menu.RenderInspector();
        }, LogMenuCallbackException);
    }
    void __stdcall Menu::SettingsCallback()
    {
        GuardCallbackVoid([] {
            if (auto* context = AcquireProcessContext()) context->menu.RenderSettings();
        }, LogMenuCallbackException);
    }
    void Menu::OnFrameworkOpen()
    {
        frameworkOpen_ = true;
        static_cast<void>(indexCoordinator_.EnsureReady());
        if (!settings_.rememberFilters) {
            ResetFiltersToDefaults();
            searchRefreshState_.Request();
        }
    }

    void Menu::OnFrameworkClosed()
    {
        frameworkOpen_ = false;
        ObserveLocationTravelClose(false);
        if (!locationTravelArmed_) pendingLocationTravel_.reset();
    }

    void Menu::OnFrameworkAfterRender()
    {
        if (!locationTravelArmed_) return;
        const auto* mainWindow = SKSEMenuFramework::GetMainWindow();
        if (mainWindow && !mainWindow->IsOpen) {
            frameworkOpen_ = false;
            ObserveLocationTravelClose(false);
        }
    }

    void Menu::RequestSessionReset() noexcept
    {
        static_cast<void>(targets_.IssueSelectionRequest());
        pendingSelectionSerial_.store(0, std::memory_order_release);
        sessionResetRequested_ = true;
    }

    SearchFilters Menu::BuildSearchFilters() const
    {
        SearchFilters filters;
        filters.selectedPlugins = selectedPluginFilters_;
        if (!exactLocationFilter_.empty()) {
            filters.location = exactLocationFilter_;
            filters.locationExact = true;
        } else if (locationFilter_.front() != '\0') {
            filters.location = locationFilter_.data();
        }
        if (aliveFilter_ != 0) filters.alive = aliveFilter_ == 1;
        if (enabledFilter_ != 0) filters.enabled = enabledFilter_ == 1;
        if (teammateFilter_ != 0) filters.teammate = teammateFilter_ == 1;
        if (potentialFollowerFilter_ != 0) {
            filters.potentialFollower = potentialFollowerFilter_ == 1;
        }
        if (loadedFilter_ != 0) filters.loaded = loadedFilter_ == 1;
        if (unknownRaceOnly_) {
            filters.unknownRaceOnly = true;
        } else if (!raceFilter_.empty()) {
            filters.race = raceFilter_;
        }
        switch (sexFilter_) {
        case 1:
            filters.sex = NpcSex::Male;
            break;
        case 2:
            filters.sex = NpcSex::Female;
            break;
        case 3:
            filters.sex = NpcSex::Unknown;
            break;
        default:
            break;
        }
        const auto knownBooleanFilter = [](int value) {
            switch (value) {
            case 1: return KnownBooleanFilter::Yes;
            case 2: return KnownBooleanFilter::No;
            case 3: return KnownBooleanFilter::Unknown;
            default: return KnownBooleanFilter::Any;
            }
        };
        filters.essential = knownBooleanFilter(essentialFilter_);
        filters.protectedActor = knownBooleanFilter(protectedFilter_);
        switch (spatialKindFilter_) {
        case 1:
            filters.spatialKind = SpatialKind::Interior;
            break;
        case 2:
            filters.spatialKind = SpatialKind::Exterior;
            break;
        case 3:
            filters.spatialKind = SpatialKind::Unknown;
            break;
        default:
            break;
        }
        switch (spatialFreshnessFilter_) {
        case 1:
            filters.spatialFreshness = SpatialFreshness::Current;
            break;
        case 2:
            filters.spatialFreshness = SpatialFreshness::LastObserved;
            break;
        case 3:
            filters.spatialFreshness = SpatialFreshness::Unavailable;
            break;
        default:
            break;
        }
        if (unknownWorldspaceOnly_) {
            filters.unknownWorldspaceOnly = true;
        } else if (worldspaceFilterFormID_ != 0) {
            filters.worldspaceFormID = worldspaceFilterFormID_;
        }
        if (unknownFactionsOnly_) {
            filters.unknownFactionsOnly = true;
        } else if (factionFilter_) {
            filters.faction = factionFilter_;
        }
        if (unknownBaseKeywordsOnly_) {
            filters.unknownBaseKeywordsOnly = true;
        } else if (baseKeywordFilter_) {
            filters.baseKeyword = baseKeywordFilter_;
        }
        filters.touchingPlugins = touchingPluginFilters_;
        filters.originalPlugins = originalPluginFilters_;
        filters.winningPlugins = winningPluginFilters_;
        filters.multiplePluginRecords = knownBooleanFilter(multiplePluginRecordsFilter_);
        if (minimumPluginRecordCount_ > 0) {
            filters.minimumPluginRecordCount = static_cast<std::size_t>(minimumPluginRecordCount_);
        }
        if (maximumPluginRecordCount_ > 0) {
            filters.maximumPluginRecordCount = static_cast<std::size_t>(maximumPluginRecordCount_);
        }
        if (unknownClassOnly_) filters.unknownClassOnly = true;
        else if (classFilter_) filters.npcClass = classFilter_;
        if (unknownVoiceTypeOnly_) filters.unknownVoiceTypeOnly = true;
        else if (voiceTypeFilter_) filters.voiceType = voiceTypeFilter_;
        if (unknownCombatStyleOnly_) filters.unknownCombatStyleOnly = true;
        else if (combatStyleFilter_) filters.combatStyle = combatStyleFilter_;
        filters.levelScaled = knownBooleanFilter(levelScalingFilter_);
        filters.favoritesOnly = favoritesOnly_;
        filters.trackedOnly = trackedOnly_;
        filters.sameLocationOnly = sameLocationOnly_;
        filters.includeGeneric = includeGeneric_;
        filters.genericOnly = genericOnly_;
        if (!settings_.enableAdvancedFilters) {
            ClearAdvancedSearchFilters(filters);
        }
        return filters;
    }

    void Menu::RunSearch(SearchRun run, bool reseedRandom)
    {
        searchRefreshState_.Select(run);
        if (reseedRandom && sortIndex_ == static_cast<int>(SortKey::Random)) {
            randomSeed_ = NextRandomSeed(randomSeed_);
        }
        searchSuggestions_.clear();
        const auto source = index_.Snapshot();
        const auto favorites = savedNpcs_.Favorites();
        std::optional<SearchTextKind> suggestionKind;
        bool suggestionHasActiveFilters = false;

        if (IncludesNpcs(searchContent_)) {
            SearchQuery query;
            query.text = searchText_.data();
            query.filters = BuildSearchFilters();
            query.sort = static_cast<SortKey>(std::clamp(sortIndex_, 0, 7));
            query.ascending = ascending_;
            query.randomSeed = randomSeed_;
            query.limit = ResultLimit(run, settings_);
            query.unlimitedResults = ResultsUnlimited(run, settings_);
            query.favoriteIdentities.reserve(favorites.size());
            for (const auto& favorite : favorites) {
                query.favoriteIdentities.push_back(favorite.identity);
            }

            auto result = Search(source, query);
            if (auto* matches = std::get_if<SearchMatches>(&result)) {
                const auto parsed = ParseSearchText(query.text);
                if (parsed) suggestionKind = parsed->kind;
                suggestionHasActiveFilters = HasActiveSearchFilters(query.filters);
                results_ = std::move(matches->visible);
                resultTotal_ = matches->total;
                resultTextMatchTotal_ = matches->textMatchTotal;
                searchError_.clear();
            } else {
                results_.clear();
                resultTotal_ = 0;
                resultTextMatchTotal_ = 0;
                searchError_ = std::get<SearchError>(std::move(result)).message;
            }
        } else {
            results_.clear();
            resultTotal_ = 0;
            resultTextMatchTotal_ = 0;
            searchError_.clear();
        }
        RunLocationSearch(run, true);
        if (run == SearchRun::Submitted && searchError_.empty() &&
            resultTotal_ + searchLocationResultTotal_ == 0) {
            const auto view = index_.Snapshot();
            logger::info(
                "Submitted search returned no visible results: query='{}', text matches={}, active filters={}, NPC catalog={}, location catalog={}",
                searchText_.data(),
                resultTextMatchTotal_,
                ActiveSearchFilterCount(BuildSearchFilters()),
                view && view->catalog ? view->catalog->size() : 0,
                view && view->locations ? view->locations->size() : 0);
        }
        if (suggestionKind && ShouldOfferTypoSuggestions(
                run,
                *suggestionKind,
                resultTotal_ + searchLocationResultTotal_,
                suggestionHasActiveFilters)) {
            searchSuggestions_ = SuggestNpcNames(source, searchText_.data(), 3);
        }
        if (!ResultsUnlimited(run, settings_)) {
            const auto limit = ResultLimit(run, settings_);
            if (LocationsAppearFirst(searchContent_, resultSectionOrder_)) {
                const auto allocation = FitCombinedResults(
                    limit, searchLocationResults_.size(), results_.size());
                searchLocationResults_.resize(allocation.primary);
                results_.resize(allocation.secondary);
            } else {
                const auto allocation = FitCombinedResults(
                    limit, results_.size(), searchLocationResults_.size());
                results_.resize(allocation.primary);
                searchLocationResults_.resize(allocation.secondary);
            }
        }
    }

    void Menu::RunLocationSearch(SearchRun run, bool mainSearch)
    {
        if (!ShouldRunLocationSearch(mainSearch, searchContent_)) {
            searchLocationResults_.clear();
            searchLocationResultTotal_ = 0;
            return;
        }
        const auto source = index_.Snapshot();
        if (!source || !source->locations) {
            if (mainSearch) {
                searchLocationResults_.clear();
                searchLocationResultTotal_ = 0;
            } else {
                locationResults_.clear();
                locationResultTotal_ = 0;
            }
            return;
        }

        LocationSearchQuery query;
        query.text = mainSearch ? searchText_.data() : locationSearchText_.data();
        if (mainSearch) query.selectedPlugins = selectedPluginFilters_;
        if (mainSearch && !exactLocationFilter_.empty()) {
            query.context = exactLocationFilter_;
            query.contextExact = true;
        } else if (mainSearch && locationFilter_.front() != '\0') {
            query.context = locationFilter_.data();
        }
        query.limit = ResultLimit(run, settings_);
        query.unlimitedResults = ResultsUnlimited(run, settings_);
        if (!mainSearch) {
            query.sort = static_cast<LocationSortKey>(std::clamp(locationSortIndex_, 0, 2));
            query.ascending = locationAscending_;
            locationSearchRefreshState_.Select(run);
        }
        const auto matches = SearchLocations(*source->locations, query);
        if (mainSearch) {
            if (IncludesLocations(searchContent_)) {
                searchLocationResults_ = matches.visible;
                searchLocationResultTotal_ = matches.total;
            } else {
                searchLocationResults_.clear();
                searchLocationResultTotal_ = 0;
            }
        } else {
            locationResults_ = matches.visible;
            locationResultTotal_ = matches.total;
        }
    }

    void Menu::RefreshForIndexGeneration()
    {
        if (sessionResetRequested_.exchange(false)) {
            tracking_.SetUninstallLocked(false);
            ResetFiltersToDefaults();
            favoriteListSearch_.fill('\0');
            recentListSearch_.fill('\0');
            trackedListSearch_.fill('\0');
            favoriteListPagination_ = {};
            recentListPagination_ = {};
            trackedListPagination_ = {};
            selected_.reset();
            selectedLocation_.reset();
            pendingLocationTravel_.reset();
            locationTravelArmed_ = false;
            locationTravelGate_.Cancel();
            locationTravelGeneration_ = 0;
            openLocationTravelConfirmation_ = false;
            locationResults_.clear();
            searchLocationResults_.clear();
            searchSuggestions_.clear();
            searchPaneRatio_ = 0.46F;
            pendingCommand_.reset();
            pendingCommandRuntimeID_ = 0;
            openCommandConfirmation_ = false;
            openTrackingWarning_ = false;
            openPrepareForUninstall_ = false;
            commandsBlocked_ = false;
            uninstallPhase_ = UninstallPhase::Idle;
            manualRefreshState_ = ManualRefreshState::Idle;
            searchAfterIndexRefresh_ = false;
            indexRefreshStatus_.clear();
            uninstallStatus_.clear();
            savedEntriesStatus_.clear();
            commandStatus_.clear();
            {
                std::scoped_lock lock(locationStatusMutex_);
                locationStatus_.clear();
            }
            {
                std::scoped_lock lock(enabledStateMutex_);
                pendingEnabledState_.reset();
            }
            seenTargetFormID_ = 0;
            selectedSource_ = TargetSource::None;
        }
        DrainTrackingCompletions();
        const auto view = index_.Snapshot();
        if (view && manualRefreshState_ == ManualRefreshState::Waiting) {
            manualRefreshState_ = ObserveManualRefresh(
                manualRefreshState_,
                manualRefreshSession_,
                manualRefreshRevision_,
                view->session,
                view->revision,
                view->readiness,
                view->failure);
            if (manualRefreshState_ == ManualRefreshState::Complete) {
                indexRefreshStatus_ = TranslateFormat(
                    "Search index refreshed. {} NPCs and {} locations available.",
                    view->catalog->size(),
                    view->locations->size());
                if (searchAfterIndexRefresh_) {
                    searchRefreshState_.Request();
                    searchAfterIndexRefresh_ = false;
                }
            } else if (manualRefreshState_ == ManualRefreshState::Failed) {
                indexRefreshStatus_ = TranslateOwned(IndexFailureText(view->failure));
                searchAfterIndexRefresh_ = false;
            }
        }
        if (view && (view->session != seenIndexSession_ || view->revision != seenIndexRevision_)) {
            seenIndexSession_ = view->session;
            seenIndexRevision_ = view->revision;
            searchRefreshState_.Request();
            locationSearchRefreshState_.Request();
            RefreshSelectedSnapshot();
            if (selectedLocation_) {
                const auto found = std::ranges::find_if(
                    *view->locations,
                    [&](const auto& location) {
                        return location.runtimeFormID == selectedLocation_->runtimeFormID &&
                            location.identity == selectedLocation_->identity;
                    });
                if (found != view->locations->end()) selectedLocation_ = *found;
                else {
                    selectedLocation_.reset();
                    SetLocationStatus("Location is no longer available.");
                }
            }
        }
    }

    void Menu::SyncSelectedTarget()
    {
        if (pendingSelectionSerial_.load(std::memory_order_acquire) != 0) return;
        const auto target = targets_.Current();
        if (!target ||
            (target->ReferenceRuntimeID() == seenTargetFormID_ && target->source == selectedSource_)) return;
        seenTargetFormID_ = target->ReferenceRuntimeID();
        selectedSource_ = target->source;

        const auto source = index_.Snapshot();
        if (!source) return;
        const auto found = std::ranges::find_if(*source->catalog, [&](const auto& npc) {
            return npc.ReferenceRuntimeID() == target->ReferenceRuntimeID();
        });
        if (found != source->catalog->end()) {
            selected_ = *found;
            return;
        }

        NpcSnapshot snapshot;
        snapshot.identity = target->identity;
        snapshot.displayName = target->displayName.empty() ? "Selected NPC" : target->displayName;
        selected_ = std::move(snapshot);
    }

    void Menu::RefreshSelectedSnapshot()
    {
        if (!selected_) return;
        const auto selectedID = selected_->ReferenceRuntimeID();
        const auto source = index_.Snapshot();
        if (!source) return;
        const auto found = std::ranges::find_if(*source->catalog, [&](const auto& npc) {
            return npc.ReferenceRuntimeID() == selectedID;
        });
        if (found != source->catalog->end()) selected_ = *found;
    }

    void Menu::QueueIndexRefresh()
    {
        const auto before = index_.Snapshot();
        manualRefreshSession_ = before ? before->session : 0;
        manualRefreshRevision_ = before ? before->revision : 0;
        switch (indexCoordinator_.ForceRefresh(IndexRequestReason::Manual)) {
        case IndexRequestResult::Queued:
            manualRefreshState_ = ManualRefreshState::Waiting;
            indexRefreshStatus_ = TranslateOwned("Search index refresh queued.");
            break;
        case IndexRequestResult::Coalesced:
            manualRefreshState_ = ManualRefreshState::Waiting;
            indexRefreshStatus_ = TranslateOwned("Search index refresh already in progress.");
            break;
        case IndexRequestResult::AlreadyReady:
            manualRefreshState_ = ManualRefreshState::Complete;
            indexRefreshStatus_ = TranslateOwned("Search index is ready.");
            if (searchAfterIndexRefresh_) {
                searchRefreshState_.Request();
                searchAfterIndexRefresh_ = false;
            }
            break;
        case IndexRequestResult::Failed:
            manualRefreshState_ = ManualRefreshState::Failed;
            indexRefreshStatus_ = TranslateOwned("Search index refresh could not start.");
            searchAfterIndexRefresh_ = false;
            break;
        }
    }

    void Menu::ApplyTrackingCompletion(TrackingCompletion completion)
    {
        static_cast<void>(operationEpoch_.RunIfCurrent(completion.epoch, [&] {
            if (completion.succeeded) {
                static_cast<void>(operationQueue_.SubmitGame(completion.epoch, [
                    this,
                    runtimeFormID = completion.runtimeFormID,
                    operation = completion.operation,
                    trackedIds = completion.trackedRuntimeFormIDs] {
                    static_cast<void>(index_.RefreshRuntimeAndTracking(runtimeFormID, trackedIds));
                    searchRefreshState_.Request();
                    logger::info(
                        "Tracking aliases applied: operation {}, FormID {:08X}, active {}",
                        static_cast<int>(operation),
                        runtimeFormID,
                        trackedIds.size());
                }));
            }

            std::string commandStatus = TranslateOwned(completion.message);
            if (completion.operation == TrackingOperation::Resume) {
                uninstallPhase_ = NextUninstallPhase(uninstallPhase_, completion.succeeded ?
                    UninstallEvent::ResumeReady : UninstallEvent::ResumeFailed);
                commandsBlocked_ = !completion.succeeded;
                tracking_.SetUninstallLocked(!completion.succeeded);
                uninstallStatus_ = commandStatus;
                if (completion.succeeded) QueueIndexRefresh();
            }
            if (completion.operation == TrackingOperation::PrepareForUninstall) {
                if (completion.succeeded) {
                    const auto sharedFavoritesCleared = favorites_.ClearForUninstall();
                    savedNpcs_.Clear();
                    const auto clearedState = savedNpcs_.SnapshotAll();
                    logger::info(
                        "Prepare for Uninstall saved-state verification: serializable state {}, favorites {}, recent {}, tracked deaths {}, warning acknowledged {}",
                        HasSerializableState(clearedState),
                        clearedState.favorites.size(),
                        clearedState.recent.size(),
                        clearedState.trackedDeaths.size(),
                        clearedState.trackingWarningAcknowledged);
                    if (!sharedFavoritesCleared) {
                        logger::error(
                            "Prepare for Uninstall could not remove shared Favorites: {}",
                            sharedFavoritesCleared.error());
                        uninstallPhase_ = NextUninstallPhase(
                            uninstallPhase_, UninstallEvent::Failure);
                        tracking_.SetUninstallLocked(false);
                        commandsBlocked_ = false;
                        uninstallStatus_ = TranslateFormat(
                            "{} Do not uninstall Whereabouts yet.",
                            sharedFavoritesCleared.error());
                        return;
                    }
                    uninstallPhase_ = NextUninstallPhase(
                        uninstallPhase_, UninstallEvent::QuestCleared);
                    uninstallStatus_ = TranslateOwned(
                        "Whereabouts cleanup completed. Make a new manual save, exit Skyrim completely, and then remove the mod. Intentional NPC movement or enabled/disabled state changes are not reverted.");
                    logger::info("Prepare for Uninstall UI lock activated");
                } else {
                    uninstallPhase_ = NextUninstallPhase(
                        uninstallPhase_, UninstallEvent::Failure);
                    tracking_.SetUninstallLocked(false);
                    commandsBlocked_ = false;
                    uninstallStatus_ = TranslateFormat(
                        "{} Do not uninstall Whereabouts yet.",
                        TranslateOwned(completion.message));
                }
            }
            if (completion.runtimeFormID != 0) {
                const auto snapshot = index_.FindSnapshot(completion.runtimeFormID);
                if (snapshot && NeedsDifferentWorldspaceTrackNotice(
                        completion.succeeded,
                        completion.operation,
                        snapshot->spatial.movementBoundary)) {
                    commandStatus = TranslateFormat(
                        "{} The NPC is in a different worldspace, so its quest marker may not appear on the map until you enter that worldspace.",
                        TranslateOwned(completion.message));
                    logger::info(
                        "Tracking worldspace notice: FormID {:08X}, worldspace {}",
                        completion.runtimeFormID,
                        snapshot->spatial.worldspace);
                }
            }
            if (completion.succeeded && selected_) RefreshSelectedSnapshot();
            SetCommandStatus(std::move(commandStatus));
            searchRefreshState_.Request();
            logger::info(
                "Tracking UI applied: operation {}, FormID {:08X}, success {}",
                static_cast<int>(completion.operation),
                completion.runtimeFormID,
                completion.succeeded);
        }));
    }

    void Menu::DrainTrackingCompletions()
    {
        auto mailbox = completions_.Drain();
        for (auto& completion : mailbox.tracking) {
            ApplyTrackingCompletion(std::move(completion));
        }
        for (const auto& completion : mailbox.enabled) {
            static_cast<void>(operationEpoch_.RunIfCurrent(completion.epoch, [&] {
                const auto pending = [&]() -> std::optional<PendingEnabledState> {
                    std::scoped_lock lock(enabledStateMutex_);
                    return pendingEnabledState_;
                }();
                if (!CanApplyEnabledStateCompletion(
                        pending,
                        completion.runtimeFormID,
                        completion.expectedEnabled,
                        true,
                        commandService_.IsEnabledStateRequestCurrent(completion.requestSerial))) {
                    return;
                }
                QueueEnabledStateRefresh(
                    completion.epoch,
                    completion.runtimeFormID,
                    completion.expectedEnabled,
                    completion.requestSerial,
                    8);
            }));
        }
    }

    void Menu::QueueUseConsoleTarget()
    {
        const auto serial = targets_.IssueSelectionRequest();
        pendingSelectionSerial_.store(0, std::memory_order_release);
        const auto token = operationEpoch_.Capture();
        if (!token || !SubmitGameTask(*token, [this, serial](OperationEpochToken) {
                if (!targets_.IsSelectionRequestCurrent(serial)) return;
                if (!targets_.UseConsoleTarget()) {
                    SetCommandStatus("No valid NPC reference is selected in the console.");
                    return;
                }
                const auto target = targets_.Current();
                if (!targets_.IsSelectionRequestCurrent(serial)) return;
                if (target) static_cast<void>(index_.RefreshRuntimeId(target->ReferenceRuntimeID()));
                if (target && target->identity.IsPersistable()) {
                    static_cast<void>(savedNpcs_.RecordRecent({
                        target->identity.StableReference(), target->displayName}));
                }
            })) SetCommandStatus("The current game session is not ready.");
    }

    void Menu::QueueUseCrosshairTarget()
    {
        const auto serial = targets_.IssueSelectionRequest();
        pendingSelectionSerial_.store(0, std::memory_order_release);
        const auto token = operationEpoch_.Capture();
        if (!token || !SubmitGameTask(*token, [this, serial](OperationEpochToken) {
                if (!targets_.IsSelectionRequestCurrent(serial)) return;
                if (!targets_.UseCrosshairTarget()) {
                    SetCommandStatus("No valid NPC reference is under the crosshair.");
                    return;
                }
                const auto target = targets_.Current();
                if (!targets_.IsSelectionRequestCurrent(serial)) return;
                if (target) static_cast<void>(index_.RefreshRuntimeId(target->ReferenceRuntimeID()));
                if (target && target->identity.IsPersistable()) {
                    static_cast<void>(savedNpcs_.RecordRecent({
                        target->identity.StableReference(), target->displayName}));
                }
            })) SetCommandStatus("The current game session is not ready.");
    }

    void Menu::ClearSelection()
    {
        static_cast<void>(targets_.IssueSelectionRequest());
        pendingSelectionSerial_.store(0, std::memory_order_release);
        selected_.reset();
        selectedLocation_.reset();
        pendingCommand_.reset();
        seenTargetFormID_ = 0;
        selectedSource_ = TargetSource::None;
        targets_.ClearAndSuppressConsoleTarget();
        SetCommandStatus("Selection cleared.");
    }

    void Menu::SelectSnapshot(const NpcSnapshot& snapshot, TargetSource source)
    {
        const auto serial = targets_.IssueSelectionRequest();
        pendingSelectionSerial_.store(serial, std::memory_order_release);
        selected_ = snapshot;
        selectedLocation_.reset();
        seenTargetFormID_ = snapshot.ReferenceRuntimeID();
        selectedSource_ = source;
        const auto token = operationEpoch_.Capture();
        const auto clearPendingSelection = [this, serial] {
            auto expected = serial;
            static_cast<void>(pendingSelectionSerial_.compare_exchange_strong(
                expected, 0, std::memory_order_acq_rel, std::memory_order_acquire));
        };
        if (!token) {
            clearPendingSelection();
            return;
        }
        if (!SubmitGameTask(*token, [this, snapshot, source, serial](OperationEpochToken) {
                const auto clearPendingSelection = [this, serial] {
                    auto expected = serial;
                    static_cast<void>(pendingSelectionSerial_.compare_exchange_strong(
                        expected, 0, std::memory_order_acq_rel, std::memory_order_acquire));
                };
                if (!targets_.IsSelectionRequestCurrent(serial)) {
                    clearPendingSelection();
                    return;
                }
                if (targets_.Select(snapshot, source) && snapshot.StableReference().IsPersistable()) {
                    if (targets_.IsSelectionRequestCurrent(serial)) {
                        static_cast<void>(savedNpcs_.RecordRecent({snapshot.StableReference(), snapshot.displayName}));
                    }
                }
                clearPendingSelection();
            })) clearPendingSelection();
    }

    void Menu::SelectLocation(const LocationSnapshot& snapshot)
    {
        static_cast<void>(targets_.IssueSelectionRequest());
        pendingSelectionSerial_.store(0, std::memory_order_release);
        targets_.ClearAndSuppressConsoleTarget();
        selected_.reset();
        selectedLocation_ = snapshot;
        pendingCommand_.reset();
        seenTargetFormID_ = 0;
        selectedSource_ = TargetSource::None;
        SetLocationStatus({});
    }

    void Menu::RenderDetails(ResultDensity density)
    {
        if (selectedLocation_) {
            RenderLocationDetails();
            return;
        }
        ImGuiMCP::SeparatorText(TranslateText("Selected NPC"));
        if (!selected_) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextUnformatted(TranslateText("No NPC selected."));
            const auto status = CommandStatus();
            if (!status.empty()) ImGuiMCP::TextWrapped("%s", status.c_str());
            return;
        }

        const auto& npc = *selected_;
        const auto favorites = savedNpcs_.Favorites();
        const bool isFavorite = npc.StableReference().IsPersistable() &&
            std::ranges::any_of(favorites, [&](const auto& entry) {
                return entry.identity == npc.StableReference();
            });
        ImGuiMCP::Spacing();
        ImGuiMCP::TextUnformatted(npc.displayName.c_str());
        const auto statusTags = StatusTagLabels(npc, isFavorite);
        const auto child = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_ChildBg);
        const auto window = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_WindowBg);
        const auto statusBackground = CompositeThemeBackground(
            {child.x, child.y, child.z}, child.w, {window.x, window.y, window.z});
        for (std::size_t index = 0; index < statusTags.size(); ++index) {
            if (index > 0) ImGuiMCP::SameLine(0.0F, 6.0F);
            const auto& tag = statusTags[index];
            const auto badge = DenseStatusBadgeForTag(tag);
            const auto semantic = badge ?
                ContrastAdjustedStatusBadgeColor(*badge, statusBackground) :
                EnsureTextContrast({0.30F, 0.58F, 0.92F}, statusBackground);
            const ImGuiMCP::ImVec4 color{
                semantic.red, semantic.green, semantic.blue, 1.0F};
            const auto localizedTag = TranslateOwned(tag);
            ImGuiMCP::TextColored(color, "[%s]", localizedTag.c_str());
            if (tag == "Generic") {
                DelayedTooltip(TranslateText(
                    "Non-unique NPC. Commands use this exact reference FormID."));
            }
        }
        if (PendingExpectedEnabled(npc.ReferenceRuntimeID())) {
            if (!statusTags.empty()) ImGuiMCP::SameLine(0.0F, 6.0F);
            ImGuiMCP::TextColored(
                {1.0F, 0.72F, 0.28F, 1.0F},
                "[%s]", TranslateText("Pending"));
            DelayedTooltip(TranslateText("Verifies after Skyrim resumes."));
        }
        ImGuiMCP::Text("%s: %08X", TranslateText("FormID"), npc.ReferenceRuntimeID());
        ImGuiMCP::SameLine();
        const auto primaryLocation = LocalizedPrimarySpatialLabel(npc.spatial);
        ImGuiMCP::Text("%s: %s", TranslateText("Location"), primaryLocation.c_str());
        const auto sourcePlugin = npc.SourcePlugin();
        ImGuiMCP::Text(
            "%s: %s",
            TranslateText("Plugin"),
            sourcePlugin.empty() ? TranslateText("Dynamic / unavailable") :
                                   std::string(sourcePlugin).c_str());

        ImGuiMCP::Spacing();
        const auto commandsHeading = std::format(
                "{}###WhereaboutsSelectedCommands", TranslateText("Commands"));
        if (ImGuiMCP::CollapsingHeader(
                    commandsHeading.c_str(),
                    ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (density == ResultDensity::Detailed) ImGuiMCP::Spacing();
            const auto* commandStyle = ImGuiMCP::GetStyle();
            const auto commandLayout = BuildFilterDensityLayout(
                density,
                ImGuiMCP::GetContentRegionAvail().x,
                180.0F,
                commandStyle ? commandStyle->ItemSpacing.x : 8.0F);
            int commandStyleVars = 0;
            if (density != ResultDensity::Detailed && commandStyle) {
                const float scale = density == ResultDensity::Compact ? 0.82F : 0.64F;
                ImGuiMCP::PushStyleVar(
                    ImGuiMCP::ImGuiStyleVar_FramePadding,
                    {commandStyle->FramePadding.x, (std::max)(1.0F, commandStyle->FramePadding.y * scale)});
                ImGuiMCP::PushStyleVar(
                    ImGuiMCP::ImGuiStyleVar_ItemSpacing,
                    {commandStyle->ItemSpacing.x, (std::max)(1.0F, commandStyle->ItemSpacing.y * scale)});
                commandStyleVars = 2;
            }
            const auto commandActions = OrderedVisibleCommandActions(settings_);
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsCommands",
                    commandLayout.commandColumns,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchSame)) {
                const auto beginCenteredCombo = [](const char* id, const char* preview) {
                    ImGuiMCP::PushStyleVar(
                        ImGuiMCP::ImGuiStyleVar_ButtonTextAlign,
                        {0.5F, 0.5F});
                    const bool open = ImGuiMCP::BeginCombo(id, preview);
                    ImGuiMCP::PopStyleVar();
                    return open;
                };
                const auto renderCommand = [&](CommandKind command) {
                    const auto check = commandPolicy_.Check(command, npc, settings_);
                    const char* label = CommandPolicy::Label(command);
                    if (command == CommandKind::Track) label = npc.tracked ? "Untrack" : "Track";
                    if (command == CommandKind::EnableDisable) label = npc.enabled ? "Disable" : "Enable";
                    if (command == CommandKind::Favorite) label = isFavorite ? "Remove Favorite" : "Add Favorite";
                    const bool trackingBusy = command == CommandKind::Track && tracking_.Busy();
                    const bool unavailable = commandsBlocked_ || trackingBusy ||
                        check.decision == CommandDecision::Unavailable;
                    ImGuiMCP::BeginDisabled(unavailable);
                    const auto localizedLabel = TranslateOwned(label);
                    const auto labelWidth = ImGuiMCP::GetContentRegionAvail().x;
                    if (ImGuiMCP::Button(localizedLabel.c_str(), {-1.0F, 0.0F})) RequestCommand(command);
                    ImGuiMCP::EndDisabled();
                    if (unavailable) {
                        const auto unavailableReason = commandsBlocked_ ?
                            TranslateOwned("Commands are unavailable because Prepare for Uninstall has cleared this save's Whereabouts state.") :
                            trackingBusy ? TranslateOwned("Tracking is updating.") :
                                           TranslateOwned(check.reason);
                        DelayedTooltip(
                            unavailableReason.c_str(),
                            ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
                    } else if (command == CommandKind::Track) {
                        DelayedTooltip(TranslateText(npc.tracked ?
                            "Removes this map and compass marker." :
                            "Adds a map and compass marker."));
                    } else if (command == CommandKind::SelectConsole) {
                        DelayedTooltip(TranslateText("Opens the console with this NPC selected."));
                    } else if (command == CommandKind::EnableDisable) {
                        DelayedTooltip(TranslateText(
                            "Changes this NPC's enabled state and verifies the result."));
                    } else {
                        OverflowTooltip(localizedLabel, labelWidth);
                    }
                };

                for (const auto& actionID : commandActions) {
                    if (actionID == kActorFlagsGroupActionID) {
                        static_cast<void>(ImGuiMCP::TableNextColumn());
                        ImGuiMCP::SetNextItemWidth(-1.0F);
                        if (beginCenteredCombo(
                                "##WhereaboutsActorFlags",
                                TranslateText("Essential / Protected"))) {
                            const auto token = operationEpoch_.Capture();
                            const auto owner = npc.recordProjection ?
                                npc.recordProjection->baseDataOwnerRuntimeFormID : 0;
                            for (const auto& flagAction : kBuiltInCommandActions) {
                                if (!IsActorFlagActionID(flagAction.id)) continue;
                                const auto command = flagAction.command;
                                const auto check = commandPolicy_.Check(command, npc, settings_);
                                const bool restoreUnavailable =
                                    command == CommandKind::RestoreOriginalFlags &&
                                    (!token || !commandService_.HasOriginalFlags(owner, *token));
                                const bool unavailable = commandsBlocked_ || restoreUnavailable ||
                                    check.decision == CommandDecision::Unavailable;
                                ImGuiMCP::BeginDisabled(unavailable);
                                if (ImGuiMCP::Selectable(
                                        TranslateText(CommandPolicy::Label(command)))) {
                                    RequestCommand(command);
                                }
                                ImGuiMCP::EndDisabled();
                                if (unavailable) {
                                    const auto reason = restoreUnavailable ?
                                        TranslateOwned("No original flags were captured this session.") :
                                        TranslateOwned(check.reason);
                                    DelayedTooltip(
                                        reason.c_str(),
                                        ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
                                }
                            }
                            ImGuiMCP::EndCombo();
                        }
                        DelayedTooltip(TranslateText(
                            "Changes independent Essential and Protected flags on the effective base NPC."));
                        continue;
                    }

                    static_cast<void>(ImGuiMCP::TableNextColumn());
                    if (actionID == kCopyIdentityActionID) {
                        ImGuiMCP::SetNextItemWidth(-1.0F);
                        if (beginCenteredCombo("##WhereaboutsCopyID", TranslateText("Copy ID"))) {
                            const auto copy = [&](CopyIdentityKind kind, const char* label) {
                                const auto localizedLabel = TranslateOwned(label);
                                if (!ImGuiMCP::Selectable(localizedLabel.c_str())) return;
                                const auto selection = ResolveCopyIdentity(npc, kind);
                                if (!selection.value.empty()) {
                                    ImGuiMCP::SetClipboardText(selection.value.c_str());
                                }
                                SetCommandStatus(LocalizedCopyConfirmation(selection));
                            };
                            copy(CopyIdentityKind::FormID, "FormID");
                            copy(CopyIdentityKind::EditorID, "EditorID");
                            copy(CopyIdentityKind::Stable, "Stable");
                            ImGuiMCP::EndCombo();
                        }
                        DelayedTooltip(TranslateText("Choose which ID to copy."));
                        continue;
                    }
                    if (actionID == kCopyNpcReportActionID) {
                        const auto localizedLabel = TranslateOwned("Copy NPC Report");
                        const auto labelWidth = ImGuiMCP::GetContentRegionAvail().x;
                        if (ImGuiMCP::Button(localizedLabel.c_str(), {-1.0F, 0.0F})) {
                            const auto report = FormatNpcReport(npc);
                            ImGuiMCP::SetClipboardText(report.c_str());
                            SetCommandStatus("Copied NPC report.");
                        }
                        OverflowTooltip(localizedLabel, labelWidth);
                        continue;
                    }
                    if (const auto customSlot = CustomSlotForActionID(actionID)) {
                        const auto& custom = settings_.customCommands[*customSlot];
                        const auto labelWidth = ImGuiMCP::GetContentRegionAvail().x;
                        if (ImGuiMCP::Button(custom.name.c_str(), {-1.0F, 0.0F})) {
                            RequestCustomCommand(*customSlot);
                        }
                        OverflowTooltip(custom.name, labelWidth);
                        continue;
                    }
                    if (const auto command = CommandForActionID(actionID)) renderCommand(*command);
                }

                ImGuiMCP::EndTable();
            }
            if (commandStyleVars > 0) ImGuiMCP::PopStyleVar(commandStyleVars);

            const auto status = CommandStatus();
            if (!status.empty()) {
                ImGuiMCP::Spacing();
                ImGuiMCP::TextWrapped("%s", status.c_str());
            }
            }

            ImGuiMCP::Spacing();
        const auto detailsHeading = std::format(
                "{}###WhereaboutsSelectedNpcDetails", TranslateText("NPC Details"));
        if (ImGuiMCP::CollapsingHeader(detailsHeading.c_str())) {
                const auto sourceLabel = TargetSourceLabel(selectedSource_);
                const auto reference = std::format(
                    "{}:{:06X}",
                    npc.StableReference().plugin.empty() ? TranslateText("Dynamic") : npc.StableReference().plugin,
                    npc.StableReference().localID);
                const auto base = std::format(
                    "{}:{:06X}",
                    npc.StableBase().plugin.empty() ? TranslateText("Unavailable") : npc.StableBase().plugin,
                    npc.StableBase().localID);
                const auto flags = std::string(npc.essential ? TranslateText("Essential") : "") +
                    (npc.essential && npc.protectedActor ? " " : "") +
                    (npc.protectedActor ? TranslateText("Protected") :
                        (npc.essential ? "" : TranslateText("None")));
                std::string originalFlags = TranslateOwned("Not captured");
                if (const auto token = operationEpoch_.Capture()) {
                    const auto owner = npc.recordProjection ?
                        npc.recordProjection->baseDataOwnerRuntimeFormID : 0;
                    if (const auto original = commandService_.OriginalFlags(owner, *token)) {
                        if (original->essential && original->protectedActor) {
                            originalFlags = std::format(
                                "{}, {}", TranslateText("Essential"), TranslateText("Protected"));
                        } else if (original->essential) {
                            originalFlags = TranslateOwned("Essential");
                        } else if (original->protectedActor) {
                            originalFlags = TranslateOwned("Protected");
                        } else {
                            originalFlags = TranslateOwned("No flags");
                        }
                    }
                }
                const auto state = std::format(
                    "{}, {}, {}",
                    TranslateText(npc.alive ? "Alive" : "Dead"),
                    TranslateText(npc.enabled ? "Enabled" : "Disabled"),
                    TranslateText(npc.loaded ? "Loaded" : "Unloaded"));
                const auto distance = npc.spatial.distance ?
                    FormatDistance(*npc.spatial.distance, settings_.distanceUnit) :
                    TranslateText("Different or unknown space");

                const auto row = [](const char* label, const std::string& value) {
                    ImGuiMCP::TableNextRow();
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
                    ImGuiMCP::TextUnformatted(label);
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                    ImGuiMCP::TextWrapped("%s", value.c_str());
                };

                ImGuiMCP::SeparatorText(TranslateText("Identity"));
                if (ImGuiMCP::BeginTable(
                        "##WhereaboutsIdentityDetails",
                        2,
                        ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
                    ImGuiMCP::TableSetupColumn(TranslateText("Label"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 175.0F);
                    ImGuiMCP::TableSetupColumn(TranslateText("Value"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
                    row(TranslateText("Source"), TranslateOwned(sourceLabel));
                    row(TranslateText("Reference"), reference);
                    row(TranslateText("Base NPC"), base);
                    row(TranslateText("Base EditorID"), npc.baseEditorID.empty() ?
                        TranslateText("Unavailable") : npc.baseEditorID);
                    row(TranslateText("Race"), npc.race.empty() ? TranslateText("Unknown") : npc.race);
                    const auto* sex = npc.sex == NpcSex::Male ? TranslateText("Male") :
                        npc.sex == NpcSex::Female ? TranslateText("Female") : TranslateText("Unknown");
                    row(TranslateText("Sex"), sex);
                    const auto* projection = npc.recordProjection.get();
                    const auto facetValue = [&](bool known, const RecordFacet& facet) {
                        if (!known) return TranslateOwned("Unknown");
                        return facet.label.empty() ? TranslateOwned("None") : facet.label;
                    };
                    row(TranslateText("Class"), projection ?
                        facetValue(projection->classKnown, projection->npcClass) : TranslateOwned("Unknown"));
                    row(TranslateText("Voice type"), projection ?
                        facetValue(projection->voiceTypeKnown, projection->voiceType) : TranslateOwned("Unknown"));
                    row(TranslateText("Combat style"), projection ?
                        facetValue(projection->combatStyleKnown, projection->combatStyle) : TranslateOwned("Unknown"));
                    std::string scaling = TranslateOwned("Unknown");
                    if (projection && projection->levelScaling.known) {
                        const auto& value = projection->levelScaling;
                        scaling = value.playerLevelMult ?
                            TranslateFormat("PC level x {:.3f} (min {}, max {})",
                                static_cast<float>(value.value) / 1000.0F,
                                value.minimum, value.maximum) :
                            TranslateFormat("Fixed level {}", value.value);
                    }
                    row(TranslateText("Level scaling"), scaling);
                    row(TranslateText("Indexed references sharing this base"),
                        std::to_string(npc.indexedReferencesSharingBase));
                    if (projection && projection->baseDataOwnerRuntimeFormID != 0) {
                        row(TranslateText("Effective Base Data owner"),
                            std::format("{:08X}", projection->baseDataOwnerRuntimeFormID));
                        row(TranslateText("Indexed references sharing this Base Data owner"),
                            std::to_string(npc.indexedReferencesSharingBaseDataOwner));
                    }
                    const bool provenanceKnown = projection && projection->provenance.known;
                    row(TranslateText("Original plugin"), provenanceKnown ?
                        projection->provenance.originalPlugin : TranslateOwned("Unknown"));
                    row(TranslateText("Winning plugin"), provenanceKnown ?
                        projection->provenance.winningPlugin : TranslateOwned("Unknown"));
                    row(TranslateText("Touch count"), provenanceKnown ?
                        std::to_string(projection->provenance.PluginRecordCount()) :
                        TranslateOwned("Unknown"));
                    ImGuiMCP::EndTable();
                }

                const auto touchingHeading = std::format(
                    "{}###WhereaboutsTouchingPlugins", TranslateText("All touching plugins"));
                if (ImGuiMCP::CollapsingHeader(touchingHeading.c_str())) {
                    const auto* projection = npc.recordProjection.get();
                    if (!projection || !projection->provenance.known) {
                        ImGuiMCP::TextDisabled("%s", TranslateText("Unknown"));
                    } else {
                        const float touchingHeight = (std::min)(
                            static_cast<float>(projection->provenance.touchingPlugins.size()),
                            7.0F) * ImGuiMCP::GetTextLineHeightWithSpacing();
                        if (ImGuiMCP::BeginChild(
                                "##WhereaboutsTouchingPluginsBody",
                                {0.0F, (std::max)(touchingHeight, ImGuiMCP::GetTextLineHeightWithSpacing())},
                                ImGuiMCP::ImGuiChildFlags_None)) {
                        for (const auto& plugin : projection->provenance.touchingPlugins) {
                            const auto availableWidth = ImGuiMCP::GetContentRegionAvail().x;
                            ImGuiMCP::BulletText("%s", plugin.c_str());
                            OverflowTooltip(plugin, availableWidth);
                        }
                        }
                        ImGuiMCP::EndChild();
                    }
                }

                ImGuiMCP::Spacing();
                ImGuiMCP::SeparatorText(TranslateText("Status"));
                if (ImGuiMCP::BeginTable(
                        "##WhereaboutsStatusDetails",
                        2,
                        ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
                    ImGuiMCP::TableSetupColumn(TranslateText("Label"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 175.0F);
                    ImGuiMCP::TableSetupColumn(TranslateText("Value"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
                    row(TranslateText("Level"), std::to_string(npc.level));
                    row(TranslateText("Health"), std::format("{:.0f}", npc.health));
                    row(TranslateText("Magicka"), std::format("{:.0f}", npc.magicka));
                    row(TranslateText("Stamina"), std::format("{:.0f}", npc.stamina));
                    row(TranslateText("Flags"), flags);
                    row(TranslateText("Original flags this session"), originalFlags);
                    row(TranslateText("State"), state);
                    row(TranslateText("Follower"), TranslateText(npc.teammate ? "Yes" : "No"));
                    row(TranslateText("Potential Follower"), TranslateText(npc.potentialFollower ? "Yes" : "No"));
                    row(TranslateText("Favorite"), TranslateText(isFavorite ? "Yes" : "No"));
                    row(TranslateText("Tracking"), TranslateText(npc.tracked ? "Active" : "Not tracked"));
                    ImGuiMCP::EndTable();
                }

                ImGuiMCP::Spacing();
                ImGuiMCP::SeparatorText(TranslateText("Location"));
                if (ImGuiMCP::BeginTable(
                        "##WhereaboutsLocationDetails",
                        2,
                        ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
                    ImGuiMCP::TableSetupColumn(TranslateText("Label"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 175.0F);
                    ImGuiMCP::TableSetupColumn(TranslateText("Value"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
                    row(TranslateText("Location"), npc.spatial.location.empty() ? TranslateText("Unavailable") : npc.spatial.location);
                    if (!npc.spatial.locationEditorID.empty() || npc.spatial.locationFormID != 0) {
                        row(TranslateText("Location record"), SpatialIdentityLabel(
                            npc.spatial.locationEditorID, npc.spatial.locationFormID));
                    }
                    if (!npc.spatial.cell.empty() && npc.spatial.cell != npc.spatial.location) {
                        row(TranslateText("Cell"), npc.spatial.cell);
                    }
                    if (!npc.spatial.cellEditorID.empty() || npc.spatial.cellFormID != 0) {
                        row(TranslateText("Cell record"), SpatialIdentityLabel(
                            npc.spatial.cellEditorID, npc.spatial.cellFormID));
                    }
                    const auto worldspace = WorldspaceSpatialLabel(npc.spatial);
                    if (!worldspace.empty() && worldspace != "Unavailable") {
                        row(TranslateText("Worldspace"), worldspace);
                    }
                    if (!npc.spatial.worldspaceEditorID.empty() || npc.spatial.worldspaceFormID != 0) {
                        row(TranslateText("Worldspace record"), SpatialIdentityLabel(
                            npc.spatial.worldspaceEditorID, npc.spatial.worldspaceFormID));
                    }
                    if (npc.spatial.kind == SpatialKind::Exterior) {
                        row(TranslateText("Exterior grid"), ExteriorCellGridLabel(npc.spatial));
                    }
                    if (npc.spatial.freshness == SpatialFreshness::LastObserved) {
                        row(TranslateText("Position"), TranslateText("Last observed; NPC is currently unloaded"));
                    }
                    row(TranslateText("Distance"), distance);
                    ImGuiMCP::EndTable();
                }
        }

    }

    void Menu::CenterNextModal()
    {
        if (const auto* viewport = ImGuiMCP::GetMainViewport()) {
            const ImGuiMCP::ImVec2 center{
                viewport->WorkPos.x + viewport->WorkSize.x * 0.5F,
                viewport->WorkPos.y + viewport->WorkSize.y * 0.5F};
            ImGuiMCP::SetNextWindowPos(
                center,
                ImGuiMCP::ImGuiCond_Appearing,
                {0.5F, 0.5F});
        }
    }

    void Menu::DelayedTooltip(const char* text, int flags)
    {
        if (!text || *text == '\0') return;
        if (ImGuiMCP::IsItemHovered(flags | ImGuiMCP::ImGuiHoveredFlags_DelayNormal)) {
            ImGuiMCP::SetTooltip("%s", TranslateText(text));
        }
    }

    void Menu::OverflowTooltip(const std::string& text, float availableWidth, int flags)
    {
        if (text.empty()) return;
        const auto width = ImGuiMCP::CalcTextSize(text.c_str()).x;
        if (NeedsOverflowTooltip(width, availableWidth)) DelayedTooltip(text.c_str(), flags);
    }

    RowInteraction Menu::BeginRowInteractionCell(const char* id, float height)
    {
        const auto contentOrigin = ImGuiMCP::GetCursorScreenPos();
        const auto* style = ImGuiMCP::GetStyle();
        const auto horizontalPadding = style ? style->CellPadding.x : 0.0F;
        const auto verticalPadding = style ? style->CellPadding.y : 0.0F;
        const ImGuiMCP::ImVec2 hitOrigin{
            contentOrigin.x - horizontalPadding,
            contentOrigin.y - verticalPadding};
        const auto width = (std::max)(
            1.0F,
            ImGuiMCP::GetContentRegionAvail().x + horizontalPadding * 2.0F);
        ImGuiMCP::SetCursorScreenPos(hitOrigin);
        const bool activated = ImGuiMCP::InvisibleButton(
            id,
            {width,
             (std::max)(height, ImGuiMCP::GetTextLineHeightWithSpacing()) +
                 verticalPadding * 2.0F},
            ImGuiMCP::ImGuiButtonFlags_AllowOverlap);
        const bool hovered = ImGuiMCP::IsItemHovered();
        ImGuiMCP::SetCursorScreenPos(contentOrigin);
        return {hovered, activated};
    }

    void Menu::ApplyUnifiedRowBackground(
        const RowInteraction& interaction,
        bool selected)
    {
        if (!selected && !interaction.hovered) return;

        const auto child = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_ChildBg);
        const auto window = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_WindowBg);
        auto background = CompositeThemeBackground(
            {child.x, child.y, child.z},
            child.w,
            {window.x, window.y, window.z});
        const auto rowColorIndex = IsAlternateTableBodyRow(ImGuiMCP::TableGetRowIndex()) ?
            ImGuiMCP::ImGuiCol_TableRowBgAlt : ImGuiMCP::ImGuiCol_TableRowBg;
        const auto row = *ImGuiMCP::GetStyleColorVec4(rowColorIndex);
        background = CompositeTableRowBackground(
            background,
            {row.x, row.y, row.z},
            row.w);
        const auto text = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_Text);
        const auto color = RowInteractionColor(
            background,
            {text.x, text.y, text.z},
            selected);
        ImGuiMCP::TableSetBgColor(
            ImGuiMCP::ImGuiTableBgTarget_RowBg0,
            ImGuiMCP::GetColorU32(
                ImGuiMCP::ImVec4{color.red, color.green, color.blue, 1.0F}));
    }

    int Menu::PushThemeSafeRowColors()
    {
        const auto row = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TableRowBg);
        const auto alternate = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TableRowBgAlt);
        if (!ShouldDeriveAlternateRowColors(
                row.w,
                alternate.w,
                ColorDistance(row, alternate))) {
            return 0;
        }

        auto background = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_ChildBg);
        const auto window = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_WindowBg);
        const auto effective = CompositeThemeBackground(
            {background.x, background.y, background.z},
            background.w,
            {window.x, window.y, window.z});
        background = {effective.red, effective.green, effective.blue, 1.0F};
        const auto accent = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_HeaderHovered);
        ImGuiMCP::ImVec4 primary{background.x, background.y, background.z, 0.08F};
        ImGuiMCP::ImVec4 striped{
            background.x * 0.88F + accent.x * 0.12F,
            background.y * 0.88F + accent.y * 0.12F,
            background.z * 0.88F + accent.z * 0.12F,
            0.16F};
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_TableRowBg, primary);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_TableRowBgAlt, striped);
        return 2;
    }

    void Menu::RenderDenseStatusBadges(
        const NpcSnapshot& npc,
        bool missing,
        bool selected,
        bool favorite)
    {
        auto badges = DenseStatusBadges(npc, favorite);
        if (missing) badges = {DenseStatusBadge::Missing};
        const auto child = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_ChildBg);
        const auto window = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_WindowBg);
        auto background = CompositeThemeBackground(
            {child.x, child.y, child.z},
            child.w,
            {window.x, window.y, window.z});
        const auto rowIndex = ImGuiMCP::TableGetRowIndex();
        const auto rowColorIndex = IsAlternateTableBodyRow(rowIndex) ?
            ImGuiMCP::ImGuiCol_TableRowBgAlt : ImGuiMCP::ImGuiCol_TableRowBg;
        const auto row = *ImGuiMCP::GetStyleColorVec4(rowColorIndex);
        background = CompositeTableRowBackground(
            background,
            {row.x, row.y, row.z},
            row.w);
        if (selected) {
            const auto text = *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_Text);
            background = RowInteractionColor(
                background,
                {text.x, text.y, text.z},
                true);
        }
        for (std::size_t index = 0; index < badges.size(); ++index) {
            if (index > 0) ImGuiMCP::SameLine(0.0F, 5.0F);
            const auto badge = badges[index];
            const auto semantic = ContrastAdjustedStatusBadgeColor(badge, background);
            const ImGuiMCP::ImVec4 color{semantic.red, semantic.green, semantic.blue, 1.0F};
            ImGuiMCP::TextColored(color, "%c", DenseStatusBadgeLetter(badge));
            const auto tooltip = TranslateOwned(DenseStatusBadgeTooltip(badge));
            DelayedTooltip(tooltip.c_str());
        }
    }

    void Menu::RequestCommand(CommandKind command)
    {
        if (!selected_) return;
        if (commandsBlocked_) {
            SetCommandStatus("Whereabouts commands are blocked after uninstall cleanup.");
            return;
        }
        if (command == CommandKind::RestoreOriginalFlags) {
            const auto token = operationEpoch_.Capture();
            const auto owner = selected_->recordProjection ?
                selected_->recordProjection->baseDataOwnerRuntimeFormID : 0;
            if (!token || !commandService_.HasOriginalFlags(owner, *token)) {
                SetCommandStatus("No original flags were captured this session.");
                return;
            }
        }
        if (command == CommandKind::EnableDisable) {
            std::optional<PendingEnabledState> pending;
            {
                std::scoped_lock lock(enabledStateMutex_);
                pending = pendingEnabledState_;
            }
            if (!CanRequestEnabledStateChange(pending, selected_->ReferenceRuntimeID())) {
                SetCommandStatus(
                    "Finish the pending Enable/Disable request before changing another NPC.");
                return;
            }
        }
        auto effective = *selected_;
        if (const auto pending = PendingExpectedEnabled(effective.ReferenceRuntimeID())) {
            effective.enabled = *pending;
        }
        const auto check = commandPolicy_.Check(command, effective, settings_);
        if (check.decision == CommandDecision::Unavailable) {
            SetCommandStatus(check.reason);
            return;
        }
        const bool trackingWarning = NeedsTrackingWarning(
                command,
                selected_->tracked,
                savedNpcs_.TrackingWarningAcknowledged());
        const auto route = ConfirmationRoute(
            settings_.showCommandConfirmations, check.decision, trackingWarning);
        if (route == CommandConfirmationRoute::TrackingWarning) {
            pendingCommand_ = command;
            pendingCommandRuntimeID_ = selected_->ReferenceRuntimeID();
            SetCommandStatus("Review the tracking save-data warning.");
            openTrackingWarning_ = true;
            return;
        }
        if (route == CommandConfirmationRoute::CommandConfirmation) {
            pendingCommand_ = command;
            pendingCommandRuntimeID_ = selected_->ReferenceRuntimeID();
            SetCommandStatus(check.reason);
            openCommandConfirmation_ = true;
            return;
        }
        const bool disabledMove =
            (command == CommandKind::Travel || command == CommandKind::Bring) &&
            !selected_->enabled;
        ExecuteCommand(command, {
            .confirmed = !settings_.showCommandConfirmations,
            .disabledMove = disabledMove ?
                DisabledMoveChoice::EnableAndMove : DisabledMoveChoice::None});
    }

    void Menu::RequestCustomCommand(std::size_t slot)
    {
        if (!selected_ || slot >= settings_.customCommands.size()) return;
        if (commandsBlocked_) {
            SetCommandStatus("Whereabouts commands are blocked after uninstall cleanup.");
            return;
        }
        const auto& custom = settings_.customCommands[slot];
        if (!custom.enabled) {
            SetCommandStatus("This custom command is disabled.");
            return;
        }
        const auto validation = ValidateCustomCommand(custom.name, custom.command);
        if (!validation) {
            SetCommandStatus(validation.error());
            return;
        }
        const auto expanded = ExpandCustomCommand(
            custom.command,
            selected_->ReferenceRuntimeID(),
            selected_->BaseRuntimeID());
        if (!expanded) {
            SetCommandStatus(expanded.error());
            return;
        }
        ExecuteCustomCommand(slot, *expanded);
    }

    void Menu::ExecuteCustomCommand(std::size_t slot, std::string expandedCommand)
    {
        if (!selected_ || slot >= settings_.customCommands.size()) return;
        const auto snapshot = *selected_;
        const auto label = settings_.customCommands[slot].name;
        const auto token = operationEpoch_.Capture();
        if (!token) {
            SetCommandStatus("The current game session is not ready.");
            return;
        }
        SetCommandStatus("Custom command queued.");
        if (!SubmitGameTask(*token, [this, snapshot, label, command = std::move(expandedCommand)](
                OperationEpochToken current) {
                auto target = targets_.Current();
                if (!target || target->ReferenceRuntimeID() != snapshot.ReferenceRuntimeID()) {
                    if (!targets_.Select(snapshot, TargetSource::Search)) {
                        SetCommandStatus("NPC is no longer available.");
                        return;
                    }
                    target = targets_.Current();
                }
                if (!target) {
                    SetCommandStatus("NPC is no longer available.");
                    return;
                }
                const auto result = commandService_.ExecuteCustomCommand(
                    *target, command, current);
                if (!result) logger::warn("Custom command '{}' failed: {}", label, result.error());
                SetCommandStatus(result ?
                    TranslateFormat("{} dispatched.", label) : result.error());
            })) {
            SetCommandStatus("The current game session is not ready.");
        }
    }

    void Menu::DispatchConfiguredAction(
        const NpcSnapshot& npc,
        TargetSource source,
        std::string_view actionID)
    {
        SelectSnapshot(npc, source);
        if (actionID.empty() || actionID == kDisabledActionID) return;
        if (actionID == kCopyNpcReportActionID) {
            const auto report = FormatNpcReport(npc);
            ImGuiMCP::SetClipboardText(report.c_str());
            SetCommandStatus("Copied NPC report.");
            return;
        }
        if (const auto custom = CustomSlotForActionID(actionID)) {
            RequestCustomCommand(*custom);
            return;
        }
        if (const auto command = CommandForActionID(actionID)) {
            RequestCommand(*command);
            return;
        }
        SetCommandStatus("The configured quick action is unavailable.");
    }

    void Menu::HandleNpcRowActivation(
        const RowInteraction& interaction,
        const NpcSnapshot& npc,
        TargetSource source)
    {
        if (interaction.activated) SelectSnapshot(npc, source);
        if (interaction.hovered &&
            ImGuiMCP::IsMouseDoubleClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
            DispatchConfiguredAction(npc, source, settings_.doubleClickAction);
        }
    }

    void Menu::RenderQuickActionButton(
        const NpcSnapshot& npc,
        TargetSource source,
        float width)
    {
        const auto& actionID = settings_.rowButtonAction;
        if (actionID == kDisabledActionID) return;
        const auto densityArea = source == TargetSource::Search ? UiDensityArea::Results :
            source == TargetSource::Inspector ? UiDensityArea::Inspector : UiDensityArea::SavedLists;
        const auto density = settings_.DensityFor(densityArea);
        const auto label = density == ResultDensity::Detailed ?
            TranslateText("Quick Action") : TranslateText("QA");
        std::string unavailable;
        if (const auto command = CommandForActionID(actionID)) {
            const auto check = commandPolicy_.Check(*command, npc, settings_);
            if (*command == CommandKind::RestoreOriginalFlags) {
                const auto token = operationEpoch_.Capture();
                const auto owner = npc.recordProjection ?
                    npc.recordProjection->baseDataOwnerRuntimeFormID : 0;
                if (!token || !commandService_.HasOriginalFlags(owner, *token)) {
                    unavailable = "No original flags were captured this session.";
                }
            }
            if (check.decision == CommandDecision::Unavailable) unavailable = check.reason;
        } else if (const auto slot = CustomSlotForActionID(actionID)) {
            const auto& custom = settings_.customCommands[*slot];
            const auto validation = ValidateCustomCommand(custom.name, custom.command);
            if (!custom.enabled || !validation) {
                unavailable = validation ? "This custom command is disabled." : validation.error();
            }
        } else if (actionID != kCopyNpcReportActionID) {
            unavailable = "The configured action is unavailable.";
        }
        ImGuiMCP::BeginDisabled(!unavailable.empty());
        if (ImGuiMCP::Button(label, {width, 0.0F})) {
            DispatchConfiguredAction(npc, source, actionID);
        }
        ImGuiMCP::EndDisabled();
        const auto tooltip = unavailable.empty() ?
            TranslateFormat("Runs: {}", ActionLabel(settings_, actionID)) :
            TranslateFormat("{} - {}", ActionLabel(settings_, actionID), unavailable);
        DelayedTooltip(
            tooltip.c_str(),
            unavailable.empty() ? 0 : ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
    }

    void Menu::RenderRootModals()
    {
        if (std::exchange(openControllerKeyboard_, false)) {
            ImGuiMCP::OpenPopup(TranslateText("Whereabouts Virtual Keyboard"));
        }
        if (std::exchange(openCommandConfirmation_, false)) {
            ImGuiMCP::OpenPopup(TranslateText("Confirm Whereabouts command"));
        }
        if (std::exchange(openTrackingWarning_, false)) {
            ImGuiMCP::OpenPopup(TranslateText("Whereabouts tracking warning"));
        }
        if (std::exchange(openPrepareForUninstall_, false)) {
            ImGuiMCP::OpenPopup(TranslateText("Prepare Whereabouts for uninstall?"));
        }
        if (std::exchange(openLocationTravelConfirmation_, false)) {
            ImGuiMCP::OpenPopup(TranslateText("Travel to this location?"));
        }
        RenderCommandConfirmation();
        RenderTrackingWarning();
        RenderPrepareForUninstall();
        RenderLocationTravelConfirmation();
        RenderControllerKeyboard();
    }

    bool Menu::RenderUninstallLockedPage()
    {
        if (!IsUninstallLocked(uninstallPhase_)) return false;
        ImGuiMCP::SeparatorText(TranslateText("Prepared for Uninstall"));
        ImGuiMCP::Spacing();
        ImGuiMCP::TextWrapped("%s", TranslateText(
            "Whereabouts is locked. Open Settings for progress and recovery instructions."));
        RenderRootModals();
        return true;
    }

    void Menu::RenderUninstallLockedSettings()
    {
        ImGuiMCP::SeparatorText(TranslateText("Prepared for Uninstall"));
        ImGuiMCP::Spacing();
        if (uninstallPhase_ == UninstallPhase::Complete) {
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Tracking markers and Whereabouts save lists are cleared. Make a new manual save, quit Skyrim completely, and remove the mod."));
        } else {
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Tracking restart has not been verified. Do not uninstall from this state. Retry Resume or load a save from before cleanup."));
        }
        ImGuiMCP::Spacing();
        ImGuiMCP::TextWrapped("%s", TranslateText(
            "Changed your mind? Resume starts Whereabouts again with empty tracking, Favorites, Recent, and tracked-death history."));
        ImGuiMCP::Spacing();
        ImGuiMCP::TextWrapped("%s", TranslateText(
            "Reloading a save while Whereabouts is still installed resumes the mod. To uninstall, save after cleanup and quit without reloading."));
        ImGuiMCP::Spacing();
        ImGuiMCP::BeginDisabled(uninstallPhase_ == UninstallPhase::Resuming);
        if (ImGuiMCP::Button(TranslateText("Resume Whereabouts"))) {
            ResumeAfterUninstallPreparation();
        }
        ImGuiMCP::EndDisabled();
        if (!uninstallStatus_.empty()) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextWrapped("%s", uninstallStatus_.c_str());
        }
    }

    void Menu::ResumeAfterUninstallPreparation()
    {
        if (uninstallPhase_ != UninstallPhase::Complete && uninstallPhase_ != UninstallPhase::ResumeFailed) return;
        const auto token = operationEpoch_.AdvanceActive();
        if (!token) {
            uninstallStatus_ = TranslateOwned(
                "The current game session is not ready. Whereabouts remains prepared for uninstall.");
            return;
        }
        commandService_.CancelPendingOperations();
        tracking_.CancelPendingOperations();
        tracking_.SetUninstallLocked(false);
        locationTravelGate_.Cancel();
        pendingLocationTravel_.reset();
        locationTravelArmed_ = false;
        locationTravelGeneration_ = 0;
        completions_.Clear();
        uninstallPhase_ = NextUninstallPhase(uninstallPhase_, UninstallEvent::Resume);
        commandsBlocked_ = true;
        uninstallStatus_ = TranslateOwned(
            "Restarting tracking. Close the SKSE menu briefly so Skyrim can finish, then reopen Settings.");
        if (!SubmitGameTask(*token, [this](OperationEpochToken current) {
                const auto resumed = tracking_.Resume(current);
                if (!resumed) completions_.PushTracking(TrackingCompletion{
                    .operation = TrackingOperation::Resume,
                    .succeeded = false,
                    .message = resumed.error(),
                    .epoch = current});
            })) {
            uninstallPhase_ = NextUninstallPhase(uninstallPhase_, UninstallEvent::ResumeFailed);
            tracking_.SetUninstallLocked(true);
            uninstallStatus_ = TranslateOwned("The current game session is not ready. Whereabouts remains prepared for uninstall.");
        }
        logger::info(
            "Whereabouts resume requested after uninstall preparation; operation epoch {}",
            token->value);
    }

    void Menu::RequestLocationTravel()
    {
        if (!selectedLocation_) return;
        locationTravelGate_.Cancel();
        pendingLocationTravel_ = *selectedLocation_;
        locationTravelArmed_ = false;
        locationTravelGeneration_ = 0;
        if (settings_.showCommandConfirmations) {
            openLocationTravelConfirmation_ = true;
        } else {
            locationTravelGeneration_ = locationTravelGate_.Arm();
            locationTravelArmed_ = true;
            SetLocationStatus("Location travel queued.");
            if (auto* mainWindow = SKSEMenuFramework::GetMainWindow()) {
                mainWindow->IsOpen = false;
            }
        }
        logger::info(
            "Location travel requested: FormID {:08X}, {}",
            pendingLocationTravel_->runtimeFormID,
            pendingLocationTravel_->displayName);
    }

    void Menu::RenderLocationTravelConfirmation()
    {
        CenterNextModal();
        if (!ImGuiMCP::BeginPopupModal(
                TranslateText("Travel to this location?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) return;

        if (!pendingLocationTravel_ || !selectedLocation_ ||
            pendingLocationTravel_->runtimeFormID != selectedLocation_->runtimeFormID) {
            pendingLocationTravel_.reset();
            locationTravelArmed_ = false;
            locationTravelGate_.Cancel();
            locationTravelGeneration_ = 0;
            ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
            return;
        }

        const auto message = TranslateFormat(
            "Travel to {}? This closes the SKSE menu.",
            pendingLocationTravel_->displayName);
        ImGuiMCP::TextWrapped("%s", message.c_str());
        ImGuiMCP::Spacing();
        if (ImGuiMCP::Button(TranslateText("Travel to Location"))) {
            locationTravelGeneration_ = locationTravelGate_.Arm();
            locationTravelArmed_ = true;
            SetLocationStatus("Location travel queued.");
            ImGuiMCP::CloseCurrentPopup();
            if (auto* mainWindow = SKSEMenuFramework::GetMainWindow()) {
                mainWindow->IsOpen = false;
            }
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Cancel"))) {
            logger::info("Location travel cancelled before framework close");
            pendingLocationTravel_.reset();
            locationTravelArmed_ = false;
            locationTravelGate_.Cancel();
            locationTravelGeneration_ = 0;
            SetLocationStatus("Location travel cancelled.");
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::EndPopup();
    }

    void Menu::ObserveLocationTravelClose(bool frameworkWindowOpen)
    {
        if (!locationTravelArmed_) return;
        if (!locationTravelGate_.TakeForDispatch(
                locationTravelGeneration_, frameworkWindowOpen)) return;
        logger::info(
            "Location travel close observed: generation {}",
            locationTravelGeneration_);
        locationTravelArmed_ = false;
        SubmitPendingLocationTravel();
    }

    void Menu::SubmitPendingLocationTravel()
    {
        if (!pendingLocationTravel_) return;
        const auto location = *pendingLocationTravel_;
        pendingLocationTravel_.reset();
        const auto generation = std::exchange(locationTravelGeneration_, 0);

        const auto token = operationEpoch_.Capture();
        logger::info(
            "Location travel dispatching: generation {}, FormID {:08X}, {}",
            generation,
            location.runtimeFormID,
            location.displayName);
        if (!token || !SubmitGameTask(*token, [this, location](OperationEpochToken) {
                auto* cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(location.runtimeFormID);
                const auto identity = TryGetFormIdentity(cell);
                if (!cell || !identity || *identity != location.identity) {
                    logger::warn(
                        "Location travel target revalidation failed: FormID {:08X}",
                        location.runtimeFormID);
                    SetLocationStatus("Location is no longer available.");
                    return;
                }
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player) {
                    SetLocationStatus("The player is not available.");
                    return;
                }
                if (player->CenterOnCell(cell)) {
                    logger::info(
                        "Location travel completed: FormID {:08X}",
                        location.runtimeFormID);
                    SetLocationStatus("Location travel completed.");
                } else {
                    logger::warn(
                        "Location travel failed in CenterOnCell: FormID {:08X}",
                        location.runtimeFormID);
                    SetLocationStatus("Skyrim could not travel to that location.");
                }
            })) {
            logger::warn("Location travel cancelled because the session is unavailable");
            SetLocationStatus("The current game session is not ready.");
        }
    }

    void Menu::SetLocationStatus(std::string status)
    {
        status = TranslateOwned(status);
        std::scoped_lock lock(locationStatusMutex_);
        locationStatus_ = std::move(status);
    }

    std::string Menu::LocationStatus() const
    {
        std::scoped_lock lock(locationStatusMutex_);
        return locationStatus_;
    }

    void Menu::RenderCommandConfirmation()
    {
        CenterNextModal();
        if (!ImGuiMCP::BeginPopupModal(
                TranslateText("Confirm Whereabouts command"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) return;

        if (!pendingCommand_ || !selected_ ||
            selected_->ReferenceRuntimeID() != pendingCommandRuntimeID_) {
            ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
            return;
        }

        ImGuiMCP::TextWrapped("%s", CommandStatus().c_str());
        const bool disabledMove =
            (*pendingCommand_ == CommandKind::Travel || *pendingCommand_ == CommandKind::Bring) &&
            !selected_->enabled;
        if (disabledMove) {
        if (ImGuiMCP::Button(TranslateText("Enable and move"))) {
                ExecuteCommand(*pendingCommand_, {true, DisabledMoveChoice::EnableAndMove});
                pendingCommand_.reset();
                pendingCommandRuntimeID_ = 0;
                ImGuiMCP::CloseCurrentPopup();
            }
        } else if (ImGuiMCP::Button(TranslateText("Confirm"))) {
            ExecuteCommand(*pendingCommand_, {true, DisabledMoveChoice::None});
            pendingCommand_.reset();
            pendingCommandRuntimeID_ = 0;
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Cancel"))) {
            pendingCommand_.reset();
            pendingCommandRuntimeID_ = 0;
            SetCommandStatus("Command cancelled.");
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::EndPopup();
    }

    void Menu::RenderTrackingWarning()
    {
        CenterNextModal();
        if (!ImGuiMCP::BeginPopupModal(
                TranslateText("Whereabouts tracking warning"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) return;

        if (!pendingCommand_ || *pendingCommand_ != CommandKind::Track || !selected_ ||
            selected_->ReferenceRuntimeID() != pendingCommandRuntimeID_) {
            pendingCommand_.reset();
            pendingCommandRuntimeID_ = 0;
            ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
            return;
        }

        ImGuiMCP::TextWrapped("%s", TranslateText(
            "Tracking stores quest-marker data in your save. Before uninstalling Whereabouts, untrack all NPCs or use Settings > Prepare for Uninstall, then make a new save and exit Skyrim."));
        ImGuiMCP::Spacing();
        if (ImGuiMCP::Button(TranslateText("Continue and Track"))) {
            savedNpcs_.AcknowledgeTrackingWarning();
            ExecuteCommand(CommandKind::Track);
            pendingCommand_.reset();
            pendingCommandRuntimeID_ = 0;
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Cancel"))) {
            pendingCommand_.reset();
            pendingCommandRuntimeID_ = 0;
            SetCommandStatus("Tracking cancelled.");
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::EndPopup();
    }

    void Menu::RequestPrepareForUninstall()
    {
        if (uninstallPhase_ == UninstallPhase::ClearingQuest) {
            uninstallStatus_ = TranslateOwned("Uninstall cleanup is already running.");
            return;
        }
        if (uninstallPhase_ == UninstallPhase::Complete) {
            uninstallStatus_ = TranslateOwned(
                "Whereabouts cleanup is already complete. Make a new manual save and exit Skyrim.");
            return;
        }
        openPrepareForUninstall_ = true;
    }

    void Menu::RenderPrepareForUninstall()
    {
        CenterNextModal();
        if (!ImGuiMCP::BeginPopupModal(
                TranslateText("Prepare Whereabouts for uninstall?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGuiMCP::TextWrapped("%s", TranslateText(
            "Untracks every NPC, clears marker objectives and saved Whereabouts lists, and shuts down tracking. After cleanup finishes, make a new manual save, quit Skyrim completely, then uninstall. NPCs previously moved or enabled/disabled are not restored."));
        ImGuiMCP::Spacing();
        if (ImGuiMCP::Button(TranslateText("Prepare for Uninstall"))) {
            const auto cleanupToken = operationEpoch_.AdvanceActive();
            if (cleanupToken) {
                uninstallPhase_ = NextUninstallPhase(uninstallPhase_, UninstallEvent::Confirm);
                commandsBlocked_ = true;
                tracking_.SetUninstallLocked(true);
                commandService_.CancelPendingOperations();
                tracking_.CancelPendingOperations();
                locationTravelGate_.Cancel();
                pendingLocationTravel_.reset();
                locationTravelArmed_ = false;
                locationTravelGeneration_ = 0;
                openCommandConfirmation_ = false;
                openTrackingWarning_ = false;
                openLocationTravelConfirmation_ = false;
                static_cast<void>(targets_.IssueSelectionRequest());
                pendingSelectionSerial_.store(0, std::memory_order_release);
                completions_.Clear();
                {
                    std::scoped_lock lock(enabledStateMutex_);
                    pendingEnabledState_.reset();
                }
                pendingCommand_.reset();
                pendingCommandRuntimeID_ = 0;
                uninstallStatus_ = TranslateOwned("Clearing Whereabouts save data and tracking quest...");
            }
            if (cleanupToken && operationQueue_.SubmitGame(*cleanupToken, [this, token = *cleanupToken] {
                    const auto result = tracking_.PrepareForUninstall(token);
                    if (!result) {
                        completions_.PushTracking({
                            .runtimeFormID = 0,
                            .operation = TrackingOperation::PrepareForUninstall,
                            .succeeded = false,
                            .message = result.error(),
                            .epoch = token});
                    }
                })) {
            } else {
                uninstallPhase_ = NextUninstallPhase(uninstallPhase_, UninstallEvent::Failure);
                tracking_.SetUninstallLocked(false);
                commandsBlocked_ = false;
                uninstallStatus_ = TranslateOwned(
                    "SKSE task interface is unavailable; do not uninstall yet.");
            }
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Cancel"))) ImGuiMCP::CloseCurrentPopup();
        ImGuiMCP::EndPopup();
    }

    void Menu::ExecuteCommand(CommandKind command, CommandOptions options)
    {
        if (!selected_) return;
        const auto snapshot = *selected_;
        const auto token = operationEpoch_.Capture();
        if (!token) {
            logger::warn(
                "{} command rejected because the current game session is suspended",
                CommandPolicy::Label(command));
            SetCommandStatus("The current game session is not ready.");
            return;
        }
        if (command == CommandKind::EnableDisable) {
            const auto expectedEnabled = NextRequestedEnabledState(
                snapshot.enabled,
                PendingExpectedEnabled(snapshot.ReferenceRuntimeID()));
            options.requestedEnabled = expectedEnabled;
            if (!operationEpoch_.RunIfCurrent(*token, [&] {
                    {
                        std::scoped_lock lock(enabledStateMutex_);
                        pendingEnabledState_ = PendingEnabledState{
                            snapshot.ReferenceRuntimeID(),
                            expectedEnabled};
                    }
                    selected_->enabled = expectedEnabled;
                    static_cast<void>(index_.SetExpectedEnabledState(
                        snapshot.ReferenceRuntimeID(),
                        expectedEnabled));
                    SetCommandStatus(expectedEnabled ?
                        "Enable requested; verification will finish after Skyrim resumes." :
                        "Disable requested; verification will finish after Skyrim resumes.");
                })) {
                return;
            }
        }
        if (command == CommandKind::MakeEssential ||
            command == CommandKind::MakeProtected ||
            command == CommandKind::RemoveFlags ||
            command == CommandKind::RestoreOriginalFlags) {
            options.baseDataOwnerRuntimeFormID = snapshot.recordProjection ?
                snapshot.recordProjection->baseDataOwnerRuntimeFormID : 0;
        }
        CloseCommandSurfaces(command);

        if (command != CommandKind::EnableDisable) SetCommandStatus("Command queued.");
        if (!SubmitGameTask(*token, [this, snapshot, command, options](OperationEpochToken token) {
            auto target = targets_.Current();
            if (!target || target->ReferenceRuntimeID() != snapshot.ReferenceRuntimeID()) {
                if (!targets_.Select(snapshot, TargetSource::Search)) {
                    if (command == CommandKind::EnableDisable && options.requestedEnabled) {
                        ClearPendingEnabled(snapshot.ReferenceRuntimeID(), *options.requestedEnabled);
                        static_cast<void>(index_.SetExpectedEnabledState(snapshot.ReferenceRuntimeID(), snapshot.enabled));
                        searchRefreshState_.Request();
                    }
                    SetCommandStatus("NPC is no longer available.");
                    return;
                }
                target = targets_.Current();
            }
            if (!target) {
                if (command == CommandKind::EnableDisable && options.requestedEnabled) {
                    ClearPendingEnabled(snapshot.ReferenceRuntimeID(), *options.requestedEnabled);
                    static_cast<void>(index_.SetExpectedEnabledState(snapshot.ReferenceRuntimeID(), snapshot.enabled));
                    searchRefreshState_.Request();
                }
                SetCommandStatus("NPC is no longer available.");
                return;
            }
            const auto result = commandService_.Execute(command, *target, options, token);
            if (!result) logger::warn("{} command failed: {}", CommandPolicy::Label(command), result.error());
            const bool queued = QueuesPapyrusWork(command, snapshot.enabled, options.disabledMove);
            if (result && command == CommandKind::EnableDisable) {
                const bool expectedEnabled = options.requestedEnabled.value_or(!snapshot.enabled);
                logger::info(
                    "Enable state requested: FormID {:08X}, expected enabled {}, menu open {}",
                    snapshot.ReferenceRuntimeID(),
                    expectedEnabled,
                    frameworkOpen_.load());
                SetCommandStatus(expectedEnabled ?
                    "Enable requested; verification will finish after Skyrim resumes." :
                    "Disable requested; verification will finish after Skyrim resumes.");
            } else if (!result && command == CommandKind::EnableDisable && options.requestedEnabled) {
                ClearPendingEnabled(snapshot.ReferenceRuntimeID(), *options.requestedEnabled);
                static_cast<void>(index_.RefreshRuntimeId(snapshot.ReferenceRuntimeID()));
                searchRefreshState_.Request();
                SetCommandStatus(result.error());
            } else {
                const auto label = TranslateOwned(CommandPolicy::Label(command));
                SetCommandStatus(result ?
                    TranslateFormat(queued ? "{} request queued." : "{} completed.", label) :
                    result.error());
            }
            if (result && !queued && command != CommandKind::EnableDisable) {
                static_cast<void>(operationQueue_.SubmitGame(
                    token,
                    [this, runtimeFormID = snapshot.ReferenceRuntimeID()] {
                        static_cast<void>(index_.RefreshRuntimeId(runtimeFormID));
                        searchRefreshState_.Request();
                    }));
            }
        })) {
            logger::warn(
                "{} command could not be queued for operation session {}",
                CommandPolicy::Label(command),
                token->value);
            static_cast<void>(operationEpoch_.RunIfCurrent(*token, [&] {
                if (command == CommandKind::EnableDisable && options.requestedEnabled) {
                    RollbackOptimisticEnabledState(snapshot, *options.requestedEnabled);
                }
                SetCommandStatus("The current game session is not ready.");
            }));
        }
    }

    void Menu::QueueEnabledStateRefresh(
        OperationEpochToken token,
        std::uint32_t runtimeFormID,
        bool expectedEnabled,
        std::uint64_t requestSerial,
        std::size_t attemptsRemaining)
    {
        const auto submitted = operationQueue_.SubmitGame(token, [
            this, token, runtimeFormID, expectedEnabled, requestSerial, attemptsRemaining] {
            const auto pending = [&]() -> std::optional<PendingEnabledState> {
                std::scoped_lock lock(enabledStateMutex_);
                return pendingEnabledState_;
            }();
            if (!CanApplyEnabledStateCompletion(
                    pending,
                    runtimeFormID,
                    expectedEnabled,
                    operationEpoch_.IsCurrent(token),
                    commandService_.IsEnabledStateRequestCurrent(requestSerial))) {
                logger::debug(
                    "Ignored stale enabled-state verification: FormID {:08X}, expected enabled {}, request {}",
                    runtimeFormID,
                    expectedEnabled,
                    requestSerial);
                return;
            }
            const auto actor = index_.ResolveRuntime(runtimeFormID);
            const bool observedEnabled = actor && !actor->IsDisabled();

            const auto action = DecideEnabledStateRefresh(
                static_cast<bool>(actor),
                observedEnabled,
                expectedEnabled,
                frameworkOpen_.load(),
                attemptsRemaining);
            logger::info(
                "Enable state observed: FormID {:08X}, expected enabled {}, observed enabled {}, "
                "menu open {}, attempts {}, action {}",
                runtimeFormID,
                expectedEnabled,
                observedEnabled,
                frameworkOpen_.load(),
                attemptsRemaining,
                static_cast<int>(action));
            if (action == StateRefreshAction::Complete) {
                static_cast<void>(index_.RefreshRuntimeId(runtimeFormID));
                searchRefreshState_.Request();
                ClearPendingEnabled(runtimeFormID, expectedEnabled);
                SetCommandStatus(expectedEnabled ? "NPC enabled." : "NPC disabled.");
                return;
            }
            if (action == StateRefreshAction::WaitForResume) {
                static_cast<void>(index_.SetExpectedEnabledState(runtimeFormID, expectedEnabled));
                searchRefreshState_.Request();
                QueueEnabledStateRefresh(
                    token, runtimeFormID, expectedEnabled, requestSerial, attemptsRemaining - 1);
                return;
            }
            if (action == StateRefreshAction::Retry) {
                QueueEnabledStateRefresh(
                    token, runtimeFormID, expectedEnabled, requestSerial, attemptsRemaining - 1);
                return;
            }
            static_cast<void>(index_.RefreshRuntimeId(runtimeFormID));
            searchRefreshState_.Request();
            ClearPendingEnabled(runtimeFormID, expectedEnabled);
            SetCommandStatus(expectedEnabled ?
                "Enable was requested, but the NPC's enabled state could not be confirmed." :
                "Disable was requested, but the NPC's disabled state could not be confirmed.");
        });
        if (!submitted) {
            SetCommandStatus("The current game session ended before state confirmation.");
        }
    }

    bool Menu::SubmitGameTask(
        OperationEpochToken token,
        std::function<void(OperationEpochToken)> task)
    {
        return operationQueue_.SubmitGame(
            token,
            [task = std::move(task), token]() mutable { task(token); }).has_value();
    }

    void Menu::RollbackOptimisticEnabledState(
        const NpcSnapshot& snapshot,
        bool expectedEnabled)
    {
        ClearPendingEnabled(snapshot.ReferenceRuntimeID(), expectedEnabled);
        if (selected_ && selected_->ReferenceRuntimeID() == snapshot.ReferenceRuntimeID()) {
            selected_->enabled = snapshot.enabled;
        }
        static_cast<void>(index_.SetExpectedEnabledState(
            snapshot.ReferenceRuntimeID(), snapshot.enabled));
        searchRefreshState_.Request();
    }

    std::optional<bool> Menu::PendingExpectedEnabled(std::uint32_t runtimeFormID) const
    {
        std::scoped_lock lock(enabledStateMutex_);
        if (!pendingEnabledState_ || pendingEnabledState_->runtimeFormID != runtimeFormID) {
            return std::nullopt;
        }
        return pendingEnabledState_->expectedEnabled;
    }

    void Menu::ClearPendingEnabled(std::uint32_t runtimeFormID, bool expectedEnabled)
    {
        std::scoped_lock lock(enabledStateMutex_);
        if (pendingEnabledState_ &&
            pendingEnabledState_->runtimeFormID == runtimeFormID &&
            pendingEnabledState_->expectedEnabled == expectedEnabled) {
            pendingEnabledState_.reset();
        }
    }

    void Menu::CloseCommandSurfaces(CommandKind command)
    {
        if (!ShouldCloseCommandSurface(command) && settings_.keepOpenAfterInlineCommand) return;
        if (auto* mainWindow = SKSEMenuFramework::GetMainWindow()) mainWindow->IsOpen = false;
    }

    void Menu::SetCommandStatus(std::string status)
    {
        status = TranslateOwned(status);
        std::scoped_lock lock(commandStatusMutex_);
        commandStatus_ = std::move(status);
    }

    std::string Menu::CommandStatus() const
    {
        std::scoped_lock lock(commandStatusMutex_);
        return commandStatus_;
    }

    std::optional<NpcSnapshot> Menu::FindSnapshot(const FormIdentity& identity) const
    {
        const auto source = index_.Snapshot();
        if (!source) return std::nullopt;
        const auto found = std::ranges::find_if(*source->catalog, [&](const auto& npc) {
            return npc.StableReference() == identity;
        });
        return found != source->catalog->end() ? std::optional{*found} : std::nullopt;
    }

    bool Menu::RenderSavedListSearch(
        std::array<char, 128>& query,
        ListPaginationState& pagination,
        const char* id,
        bool actionRendered,
        float toolbarWidth,
        float actionWidth)
    {
        const auto* style = ImGuiMCP::GetStyle();
        const float spacing = style ? style->ItemSpacing.x : 8.0F;
        const auto placement = actionRendered ? ChooseListSearchPlacement(
            toolbarWidth, actionWidth, 180.0F, spacing) : ListSearchPlacement::NextLine;
        if (actionRendered && placement == ListSearchPlacement::BesideAction) {
            ImGuiMCP::SameLine();
        }
        const auto available = placement == ListSearchPlacement::BesideAction ?
            (std::max)(1.0F, toolbarWidth - actionWidth - spacing) :
            (std::max)(1.0F, ImGuiMCP::GetContentRegionAvail().x);
        ImGuiMCP::SetNextItemWidth((std::min)(360.0F, available));
        if (!ImGuiMCP::InputTextWithHint(id, TranslateText("Search list..."), query.data(), query.size())) {
            return false;
        }
        ResetListPaginationForQuery(pagination);
        return true;
    }

    void Menu::RenderSavedListPagination(
        const char* id,
        std::size_t filteredCount,
        std::size_t pageCapacity,
        ListPaginationState& pagination)
    {
        if (filteredCount == 0) return;
        const auto page = VisibleSavedListPage(filteredCount, pageCapacity, pagination);
        ImGuiMCP::PushID(id);
        ImGuiMCP::BeginDisabled(pagination.showAll || page.page == 0);
        if (ImGuiMCP::Button(TranslateText("Previous"))) --pagination.page;
        ImGuiMCP::EndDisabled();
        ImGuiMCP::SameLine();
        ImGuiMCP::BeginDisabled(pagination.showAll || page.page + 1 >= page.pageCount);
        if (ImGuiMCP::Button(TranslateText("Next"))) ++pagination.page;
        ImGuiMCP::EndDisabled();
        ImGuiMCP::SameLine();
        const auto pageLabel = TranslateFormat("Page {} of {}", page.page + 1, page.pageCount);
        ImGuiMCP::TextUnformatted(pageLabel.c_str());
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Checkbox(TranslateText("Show all"), &pagination.showAll)) {
            pagination.page = 0;
        }
        ImGuiMCP::PopID();
    }

    void Menu::RenderSavedEntries(
        const std::vector<SavedNpcEntry>& entries,
        TargetSource source,
        std::string_view query,
        ListPaginationState& pagination)
    {
        const auto density = settings_.DensityFor(UiDensityArea::SavedLists);
        if (entries.empty()) {
            NormalizeListPagination(pagination, 0, 1, density);
            ImGuiMCP::TextUnformatted(TranslateText("No entries."));
            return;
        }

        const auto favoriteEntries = savedNpcs_.Favorites();
        const auto savedView = index_.Snapshot();
        const bool indexReady = IsCurrentIndexView(savedView, index_.CurrentSession()) &&
            savedView->readiness == IndexReadiness::Ready;
        std::vector<FormIdentity> unavailable;
        unavailable.reserve(entries.size());
        for (const auto& entry : entries) {
            if (indexReady && !FindSnapshot(entry.identity) && !index_.IsPluginLoaded(entry.identity.plugin)) {
                unavailable.push_back(entry.identity);
            }
        }
        const auto* cleanupStyle = ImGuiMCP::GetStyle();
        const float cleanupPadding = cleanupStyle ? cleanupStyle->FramePadding.x : 4.0F;
        ImGuiMCP::BeginDisabled(unavailable.empty());
        const bool removeUnavailable = ImGuiMCP::Button(TranslateText("Remove Unavailable Entries"));
        ImGuiMCP::EndDisabled();
        DelayedTooltip(TranslateText("Removes entries from missing plugins. Temporarily unavailable NPCs are kept."));
        if (removeUnavailable) {
            std::size_t removed = 0;
            std::string removalError;
            const auto token = operationEpoch_.Capture();
            if (token) static_cast<void>(operationEpoch_.RunIfCurrent(*token, [&] {
                const auto current = index_.Snapshot();
                if (!IsCurrentIndexView(current, index_.CurrentSession()) ||
                    current->readiness != IndexReadiness::Ready || current->session != savedView->session) return;
                std::erase_if(unavailable, [&](const auto& identity) {
                    return index_.IsPluginLoaded(identity.plugin) || FindSnapshot(identity).has_value();
                });
                if (source == TargetSource::Favorite) {
                    const auto result = favorites_.RemoveMany(unavailable);
                    if (result) removed = *result;
                    else removalError = result.error();
                } else {
                    removed = savedNpcs_.RemoveRecent(unavailable);
                }
            }));
            savedEntriesStatus_ = removalError.empty() ?
                TranslateFormat(
                    removed == 1 ? "Removed {} unavailable entry." : "Removed {} unavailable entries.",
                    removed) :
                removalError;
            return;
        }
        if (!savedEntriesStatus_.empty()) {
            ImGuiMCP::SameLine();
            ImGuiMCP::TextUnformatted(savedEntriesStatus_.c_str());
        }

        std::vector<SavedListSearchDocument> searchDocuments;
        searchDocuments.reserve(entries.size());
        for (const auto& entry : entries) {
            const auto snapshot = FindSnapshot(entry.identity);
            searchDocuments.push_back(snapshot ? SavedListDocument(*snapshot) : SavedListDocument(entry));
        }
        const auto filteredRows = FilterSavedListRows(searchDocuments, query);
        const bool showSecondary = ShowsSecondaryResultMetadata(density);
        const float measuredLineHeight = ImGuiMCP::GetTextLineHeightWithSpacing();
        const float rowHeight = measuredLineHeight * (showSecondary ? 2.0F : 1.0F);
        const auto pageCapacity = SavedListPageCapacity(
            density, measuredLineHeight * 10.0F, rowHeight);
        NormalizeListPagination(
            pagination, filteredRows.size(), pageCapacity, density);
        if (filteredRows.empty()) {
            ImGuiMCP::TextUnformatted(TranslateText("No matching entries."));
            return;
        }
        const auto page = VisibleSavedListPage(filteredRows.size(), pageCapacity, pagination);

        const auto pushedColors = PushThemeSafeRowColors();
        const bool superCompact = IsSuperCompactResultDensity(density);
        if (!ImGuiMCP::BeginTable(
                superCompact ? "##WhereaboutsSuperCompactSavedEntries" :
                               "##WhereaboutsSavedEntries",
                superCompact ? 3 : 4,
                ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_BordersInnerH |
                    ImGuiMCP::ImGuiTableFlags_SizingStretchProp |
                    ImGuiMCP::ImGuiTableFlags_Resizable |
                    ImGuiMCP::ImGuiTableFlags_Reorderable)) {
            if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
            return;
        }
        ImGuiMCP::TableSetupColumn(
            TranslateText("NPC"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch,
            superCompact ? 0.70F : 0.43F);
        if (superCompact) {
            ImGuiMCP::TableSetupColumn(
                TranslateText("FormID"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 110.0F);
        } else {
            ImGuiMCP::TableSetupColumn(
                TranslateText("Status"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch, 0.12F);
            ImGuiMCP::TableSetupColumn(
                TranslateText("Location"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch, 0.31F);
        }
        ImGuiMCP::TableSetupColumn(
            TranslateText("Action"),
            ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
            settings_.rowButtonAction == kDisabledActionID ? 90.0F :
                density == ResultDensity::Detailed ? 225.0F : 145.0F);
        ImGuiMCP::TableHeadersRow();

        for (std::size_t pageIndex = 0; pageIndex < page.count; ++pageIndex) {
            const auto index = filteredRows[page.first + pageIndex];
            const auto& entry = entries[index];
            const auto snapshot = FindSnapshot(entry.identity);
            const bool selected = snapshot && selected_ &&
                selected_->ReferenceRuntimeID() == snapshot->ReferenceRuntimeID();
            const auto availability = ClassifySavedEntryAvailability(
                snapshot.has_value(), index_.IsPluginLoaded(entry.identity.plugin));
            ImGuiMCP::PushID(static_cast<int>(index));
            ImGuiMCP::TableNextRow(0, rowHeight);
            RowInteraction interaction;
            static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
            const auto nameHit = BeginRowInteractionCell("##savedNpcCell", rowHeight);
            interaction.Include(nameHit.hovered, nameHit.activated);
            const auto name = entry.lastKnownName.empty() ? entry.identity.plugin : entry.lastKnownName;
            const auto nameWidth = ImGuiMCP::GetContentRegionAvail().x;
            ImGuiMCP::TextUnformatted(name.c_str());
            if (superCompact) {
                OverflowTooltip(name, nameWidth);
                static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                const auto formIdHit = BeginRowInteractionCell(
                    "##savedFormIdCell", rowHeight);
                interaction.Include(formIdHit.hovered, formIdHit.activated);
                if (snapshot) {
                    ImGuiMCP::Text("%08X", snapshot->ReferenceRuntimeID());
                } else {
                    ImGuiMCP::Text("%06X", entry.identity.localID);
                }
            } else {
            const auto identity = snapshot ?
                std::format("{} / {:08X}",
                    snapshot->SourcePlugin().empty() ? "Dynamic" : snapshot->SourcePlugin(),
                    snapshot->ReferenceRuntimeID()) :
                std::format("{}:{:06X}", entry.identity.plugin, entry.identity.localID);
            if (showSecondary) {
                OverflowTooltip(name, nameWidth);
                const auto identityWidth = ImGuiMCP::GetContentRegionAvail().x;
                ImGuiMCP::TextColored(
                    *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                    "%s",
                    identity.c_str());
                OverflowTooltip(identity, identityWidth);
            } else {
                const auto tooltip = std::format("{}\n{}", name, identity);
                DelayedTooltip(tooltip.c_str());
            }

            static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
            const auto statusHit = BeginRowInteractionCell("##savedStatusCell", rowHeight);
            interaction.Include(statusHit.hovered, statusHit.activated);
            if (snapshot) {
                const bool favorite = snapshot->StableReference().IsPersistable() &&
                    std::ranges::any_of(favoriteEntries, [&](const auto& favoriteEntry) {
                        return favoriteEntry.identity == snapshot->StableReference();
                    });
                RenderDenseStatusBadges(*snapshot, false, selected, favorite);
            } else {
                const auto status = TranslateText(
                    availability == SavedEntryAvailability::PluginMissing ?
                        "Plugin missing" : "NPC unavailable");
                ImGuiMCP::TextColored(
                    *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                    "%s",
                    status);
            }
            static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
            const auto locationHit = BeginRowInteractionCell("##savedLocationCell", rowHeight);
            interaction.Include(locationHit.hovered, locationHit.activated);
            if (snapshot) {
                const auto location = LocalizedPrimarySpatialLabel(snapshot->spatial);
                const auto locationWidth = ImGuiMCP::GetContentRegionAvail().x;
                ImGuiMCP::TextUnformatted(location.c_str());
                const auto worldspace = SecondaryWorldspaceLabel(snapshot->spatial);
                if (showSecondary && !worldspace.empty()) {
                    OverflowTooltip(location, locationWidth);
                    const auto worldspaceWidth = ImGuiMCP::GetContentRegionAvail().x;
                    ImGuiMCP::TextColored(
                        *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                        "%s",
                        worldspace.c_str());
                    OverflowTooltip(worldspace, worldspaceWidth);
                } else if (!showSecondary) {
                    const auto tooltip = worldspace.empty() ? location :
                        std::format("{}\n{}", location, worldspace);
                    DelayedTooltip(tooltip.c_str());
                }
            } else {
                ImGuiMCP::TextUnformatted("-");
            }
            }
            ApplyUnifiedRowBackground(interaction, selected);
            if (snapshot) HandleNpcRowActivation(interaction, *snapshot, source);
            static_cast<void>(ImGuiMCP::TableSetColumnIndex(superCompact ? 2 : 3));
            if (snapshot && settings_.rowButtonAction != kDisabledActionID) {
                const auto width = (std::max)(
                    52.0F,
                    ImGuiMCP::GetContentRegionAvail().x * 0.58F);
                RenderQuickActionButton(*snapshot, source, width);
                ImGuiMCP::SameLine();
            }
            if (ImGuiMCP::Button(TranslateText("Remove"))) {
                if (source == TargetSource::Favorite) {
                    if (const auto removed = favorites_.Remove(entry.identity); !removed) {
                        savedEntriesStatus_ = removed.error();
                    }
                } else if (source == TargetSource::Recent) {
                    static_cast<void>(savedNpcs_.RemoveRecent(entry.identity));
                }
                ImGuiMCP::PopID();
                ImGuiMCP::EndTable();
                if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
                return;
            }
            ImGuiMCP::PopID();
        }
        ImGuiMCP::EndTable();
        if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
        RenderSavedListPagination(
            source == TargetSource::Favorite ? "##FavoritesPagination" : "##RecentPagination",
            filteredRows.size(),
            pageCapacity,
            pagination);
    }

    void Menu::SaveSettings()
    {
        settings_.Normalize();
        spdlog::set_level(settings_.debugLogging ? spdlog::level::debug : spdlog::level::info);
        savedNpcs_.SetRecentLimit(settings_.recentLimit);
        const auto saved = settingsRepository_.Save(settings_);
        runtimeSettings_.Publish(settings_);
        searchRefreshState_.Request();
        locationSearchRefreshState_.Request();
        settingsStatus_ = saved ? std::string{} : TranslateFormat("Settings error: {}", saved.error());
    }

    void Menu::ResetAdvancedFilters()
    {
        raceFilter_.clear();
        unknownRaceOnly_ = false;
        sexFilter_ = 0;
        essentialFilter_ = 0;
        protectedFilter_ = 0;
        spatialKindFilter_ = 0;
        spatialFreshnessFilter_ = 0;
        worldspaceFilterFormID_ = 0;
        unknownWorldspaceOnly_ = false;
        worldspaceFilterLabel_.clear();
        factionFilter_.reset();
        unknownFactionsOnly_ = false;
        factionFilterLabel_.clear();
        baseKeywordFilter_.reset();
        unknownBaseKeywordsOnly_ = false;
        baseKeywordFilterLabel_.clear();
        touchingPluginFilters_.clear();
        originalPluginFilters_.clear();
        winningPluginFilters_.clear();
        touchingPluginSearch_.fill('\0');
        originalPluginSearch_.fill('\0');
        winningPluginSearch_.fill('\0');
        multiplePluginRecordsFilter_ = 0;
        minimumPluginRecordCount_ = 0;
        maximumPluginRecordCount_ = 0;
        classFilter_.reset();
        unknownClassOnly_ = false;
        classFilterLabel_.clear();
        voiceTypeFilter_.reset();
        unknownVoiceTypeOnly_ = false;
        voiceTypeFilterLabel_.clear();
        combatStyleFilter_.reset();
        unknownCombatStyleOnly_ = false;
        combatStyleFilterLabel_.clear();
        levelScalingFilter_ = 0;
        factionOptionSearch_.fill('\0');
        keywordOptionSearch_.fill('\0');
        classOptionSearch_.fill('\0');
        voiceTypeOptionSearch_.fill('\0');
        combatStyleOptionSearch_.fill('\0');
        visibleFactionOptions_ = {};
        visibleKeywordOptions_ = {};
        visibleClassOptions_ = {};
        visibleVoiceTypeOptions_ = {};
        visibleCombatStyleOptions_ = {};
        factionOptionResultsDirty_ = true;
        keywordOptionResultsDirty_ = true;
        classOptionResultsDirty_ = true;
        voiceTypeOptionResultsDirty_ = true;
        combatStyleOptionResultsDirty_ = true;
    }

    void Menu::ResetFiltersToDefaults()
    {
        pluginFilter_.fill('\0');
        selectedPluginFilters_.clear();
        locationFilter_.fill('\0');
        exactLocationFilter_.clear();
        aliveFilter_ = 0;
        enabledFilter_ = 0;
        teammateFilter_ = 0;
        potentialFollowerFilter_ = 0;
        loadedFilter_ = 0;
        ResetAdvancedFilters();
        favoritesOnly_ = false;
        trackedOnly_ = false;
        sameLocationOnly_ = false;
        searchContent_ = SearchContent::NpcsOnly;
        resultSectionOrder_ = ResultSectionOrder::NpcsFirst;
        includeGeneric_ = false;
        genericOnly_ = false;
        genericAutoContext_ = false;
        sortIndex_ = static_cast<int>(SortKey::Name);
        ascending_ = true;
        searchSortUiDirty_ = true;
    }
}
