#include "PCH.h"

#include "Commands/CommandPolicy.h"
#include "Core/FormIdentityAdapter.h"
#include "Core/SpatialPresentation.h"
#include "Core/SnapshotState.h"
#include "Core/TextFold.h"
#include "Search/EditorIdLookup.h"
#include "Search/RuntimeIndex.h"
#include "Targets/TargetSelection.h"

#include <chrono>
#include <expected>
#include <string_view>
#include <mutex>
#include <vector>

#ifdef GetObject
#undef GetObject
#endif

namespace whereabouts
{
    namespace
    {
        bool IsSearchableActor(const RE::Actor* actor)
        {
            const auto* base = actor ? actor->GetActorBase() : nullptr;
            return IsEligibleNpcTarget(
                actor != nullptr,
                actor && actor->IsPlayerRef(),
                base != nullptr);
        }

        template <class Form>
        std::string CopyDisplayName(const Form* form)
        {
            if (!form) return {};
            if (const auto* name = form->GetFullName(); name && std::string_view(name).size() > 0) {
                return name;
            }
            return {};
        }

        template <class Form>
        std::string CopyFormName(const Form* form)
        {
            if (!form) return {};
            if (auto name = CopyDisplayName(form); !name.empty()) return name;
            if (const auto* editorID = form->GetFormEditorID(); editorID && std::string_view(editorID).size() > 0) {
                return editorID;
            }
            return {};
        }

        std::string CopyEditorID(const RE::TESForm* form)
        {
            if (!form) return {};
            const auto* editorID = form->GetFormEditorID();
            return editorID && *editorID ? std::string(editorID) : std::string{};
        }

        void CaptureSpatialIdentity(
            const RE::TESForm* form,
            std::uint32_t& runtimeFormID,
            std::string& editorID)
        {
            if (!form) return;
            runtimeFormID = form->GetFormID();
            editorID = CopyEditorID(form);
        }

        std::optional<float> DistanceFromPlayer(const RE::Actor& actor)
        {
            const auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return std::nullopt;

            const auto* actorWorld = actor.GetWorldspace();
            const auto* playerWorld = player->GetWorldspace();
            const bool sameExterior = actorWorld && actorWorld == playerWorld;
            const bool sameInterior = !actorWorld && !playerWorld &&
                                      actor.GetParentCell() == player->GetParentCell();
            if (!sameExterior && !sameInterior) return std::nullopt;
            return actor.GetPosition().GetDistance(player->GetPosition());
        }

        SpatialSnapshot CaptureSpatial(RE::Actor& actor)
        {
            SpatialSnapshot spatial;
            auto* cell = actor.GetParentCell();
            if (!cell) cell = actor.GetSaveParentCell();

            const auto* exactLocation = actor.GetCurrentLocation();
            if (exactLocation) {
                spatial.locationSource = LocationSource::Actor;
            } else if (cell) {
                exactLocation = cell->GetLocation();
                if (exactLocation) spatial.locationSource = LocationSource::Cell;
            }

            const auto* displayLocation = exactLocation;
            spatial.location = CopyFormName(displayLocation);
            if (displayLocation && spatial.location.empty()) {
                std::array<const RE::BGSLocation*, 16> visited{};
                std::size_t count = 0;
                auto* parent = displayLocation->parentLoc;
                while (parent && count < visited.size()) {
                    if (std::ranges::find(visited.begin(), visited.begin() + count, parent) !=
                        visited.begin() + count) {
                        break;
                    }
                    visited[count++] = parent;
                    spatial.location = CopyFormName(parent);
                    if (!spatial.location.empty()) {
                        displayLocation = parent;
                        spatial.locationSource = LocationSource::NamedParent;
                        break;
                    }
                    parent = parent->parentLoc;
                }
            }

            CaptureSpatialIdentity(
                displayLocation,
                spatial.locationFormID,
                spatial.locationEditorID);

            spatial.cell = CopyFormName(cell);
            CaptureSpatialIdentity(cell, spatial.cellFormID, spatial.cellEditorID);
            if (cell) {
                spatial.kind = cell->IsInteriorCell() ? SpatialKind::Interior : SpatialKind::Exterior;
                if (spatial.kind == SpatialKind::Exterior) {
                    if (const auto* coordinates = cell->GetCoordinates()) {
                        spatial.exteriorCellX = coordinates->cellX;
                        spatial.exteriorCellY = coordinates->cellY;
                    }
                }
            }
            const auto* actorWorldspace = actor.GetWorldspace();
            const auto* cellWorldspace = cell && cell->IsExteriorCell() ?
                cell->GetRuntimeData().worldSpace : nullptr;
            if (spatial.kind == SpatialKind::Exterior) {
                const auto actorWorldspaceName = CopyFormName(actorWorldspace);
                const auto cellWorldspaceName = CopyFormName(cellWorldspace);
                spatial.worldspace = PreferredWorldspaceName(
                    actorWorldspaceName, cellWorldspaceName);
                const auto* displayedWorldspace = !actorWorldspaceName.empty() ?
                    actorWorldspace : (!cellWorldspaceName.empty() ? cellWorldspace :
                        (actorWorldspace ? actorWorldspace : cellWorldspace));
                CaptureSpatialIdentity(
                    displayedWorldspace,
                    spatial.worldspaceFormID,
                    spatial.worldspaceEditorID);
            }

            const auto* player = RE::PlayerCharacter::GetSingleton();
            const auto* playerCell = player ? player->GetParentCell() : nullptr;
            if (!playerCell && player) playerCell = player->GetSaveParentCell();
            const auto* playerLocation = player ? player->GetCurrentLocation() : nullptr;
            if (!playerLocation && playerCell) playerLocation = playerCell->GetLocation();
            if (!actorWorldspace) actorWorldspace = cellWorldspace;
            const auto* playerWorldspace = player ? player->GetWorldspace() : nullptr;
            if (!playerWorldspace && playerCell && playerCell->IsExteriorCell()) {
                playerWorldspace = playerCell->GetRuntimeData().worldSpace;
            }

            spatial.sameCell = player && cell && cell == playerCell;
            spatial.sameLocation = player && exactLocation && exactLocation == playerLocation;
            spatial.sameWorldspace = player && actorWorldspace &&
                actorWorldspace == playerWorldspace;
            spatial.movementBoundary = player ?
                ClassifyMovementBoundary(
                    actorWorldspace != nullptr,
                    playerWorldspace != nullptr,
                    actorWorldspace != nullptr && actorWorldspace == playerWorldspace,
                    cell != nullptr,
                    playerCell != nullptr,
                    cell != nullptr && cell == playerCell) :
                MovementBoundary::Unknown;
            spatial.distance = DistanceFromPlayer(actor);
            spatial.freshness = SpatialFreshness::Current;
            return spatial;
        }

        void RefreshSearchKeys(NpcSnapshot& snapshot)
        {
            snapshot.searchKeys.name = FoldTextForSearch(snapshot.displayName);
            snapshot.searchKeys.editorIDs = FoldTextForSearch(
                snapshot.referenceEditorID + " " + snapshot.baseEditorID);
            snapshot.searchKeys.plugin = FoldTextForSearch(snapshot.SourcePlugin());
            snapshot.searchKeys.location = FoldTextForSearch(SearchableSpatialText(snapshot.spatial));
        }

        class FullBuildTimer
        {
        public:
            explicit FullBuildTimer(std::atomic_uint64_t& destination) noexcept :
                destination_(destination),
                started_(std::chrono::steady_clock::now())
            {}

            ~FullBuildTimer()
            {
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - started_);
                destination_.store(
                    static_cast<std::uint64_t>(elapsed.count()),
                    std::memory_order_relaxed);
            }

        private:
            std::atomic_uint64_t& destination_;
            std::chrono::steady_clock::time_point started_;
        };

        EditorIdLookup CaptureEditorIds()
        {
            EditorIdLookup result;
            const auto& [forms, lock] = RE::TESForm::GetAllFormsByEditorID();
            const RE::BSReadLockGuard guard{lock};
            if (!forms) return result;

            for (const auto& [editorID, form] : *forms) {
                if (!form) continue;
                const auto type = form->GetFormType();
                if (type != RE::FormType::ActorCharacter && type != RE::FormType::NPC) continue;
                result.Observe(form->GetFormID(), editorID.data());
            }
            return result;
        }

        std::expected<std::vector<NpcSnapshot>, IndexFailure> CaptureGameCatalog(
            RuntimeIndex& index)
        {
            const auto editorIds = CaptureEditorIds();
            std::vector<RE::FormID> candidateIDs;
            {
                const auto& [forms, lock] = RE::TESForm::GetAllForms();
                const RE::BSReadLockGuard guard{lock};
                if (!forms) return std::unexpected(IndexFailure::FormsUnavailable);
                candidateIDs.reserve(forms->size());
                for (const auto& [formID, form] : *forms) {
                    if (form && form->GetFormType() == RE::FormType::ActorCharacter) {
                        candidateIDs.push_back(formID);
                    }
                }
            }

            std::vector<NpcSnapshot> rebuilt;
            rebuilt.reserve(candidateIDs.size());
            for (const auto formID : candidateIDs) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
                if (!IsSearchableActor(actor)) continue;

                const auto* displayName = actor->GetDisplayFullName();
                if (!displayName || std::string_view(displayName).empty()) continue;

                NpcSnapshot snapshot;
                snapshot.identity.reference.runtimeFormID = actor->GetFormID();
                if (const auto* base = actor->GetActorBase()) {
                    snapshot.identity.base.runtimeFormID = base->GetFormID();
                    snapshot.identity.uniqueBase = base->IsUnique();
                }
                snapshot.displayName = displayName;
                if (const auto identity = TryGetFormIdentity(actor)) {
                    snapshot.identity.reference.stable = *identity;
                }
                if (!snapshot.identity.IsPersistable()) continue;
                if (const auto identity = TryGetFormIdentity(actor->GetActorBase())) {
                    snapshot.identity.base.stable = *identity;
                }
                snapshot.referenceEditorID = editorIds.Find(snapshot.identity.reference.runtimeFormID);
                snapshot.baseEditorID = editorIds.Find(snapshot.identity.base.runtimeFormID);
                index.RefreshDynamic(snapshot);
                rebuilt.push_back(std::move(snapshot));
            }
            return rebuilt;
        }

        std::expected<std::vector<LocationSnapshot>, IndexFailure> CaptureGameLocations()
        {
            std::vector<RE::FormID> candidateIDs;
            {
                const auto& [forms, lock] = RE::TESForm::GetAllForms();
                const RE::BSReadLockGuard guard{lock};
                if (!forms) return std::unexpected(IndexFailure::FormsUnavailable);
                candidateIDs.reserve(forms->size());
                for (const auto& [formID, form] : *forms) {
                    if (form && form->GetFormType() == RE::FormType::Cell) {
                        candidateIDs.push_back(formID);
                    }
                }
            }

            std::vector<LocationSnapshot> rebuilt;
            rebuilt.reserve(candidateIDs.size());
            for (const auto formID : candidateIDs) {
                auto* cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(formID);
                if (!cell) continue;
                const auto identity = TryGetFormIdentity(cell);
                const auto name = CopyFormName(cell);
                const auto editorID = CopyEditorID(cell);
                if (!IsLocationCatalogCandidate(
                        cell->IsDeleted(), identity.has_value(), !name.empty(), !editorID.empty())) {
                    continue;
                }

                LocationSnapshot snapshot;
                snapshot.runtimeFormID = cell->GetFormID();
                snapshot.identity = *identity;
                snapshot.editorID = editorID;
                snapshot.interior = cell->IsInteriorCell();

                const auto* location = cell->GetLocation();
                const auto locationEditorID = CopyEditorID(location);
                snapshot.containingLocation = CopyDisplayName(location);
                if (location && snapshot.containingLocation.empty()) {
                    std::array<const RE::BGSLocation*, 16> visited{};
                    std::size_t count = 0;
                    auto* parent = location->parentLoc;
                    while (parent && count < visited.size()) {
                        if (std::ranges::find(visited.begin(), visited.begin() + count, parent) !=
                            visited.begin() + count) break;
                        visited[count++] = parent;
                        snapshot.containingLocation = CopyDisplayName(parent);
                        if (!snapshot.containingLocation.empty()) break;
                        parent = parent->parentLoc;
                    }
                }
                if (snapshot.containingLocation.empty()) {
                    snapshot.containingLocation = locationEditorID;
                }

                if (cell->IsExteriorCell()) {
                    snapshot.worldspace = CopyFormName(cell->GetRuntimeData().worldSpace);
                    if (const auto* coordinates = cell->GetCoordinates()) {
                        snapshot.hasExteriorGrid = true;
                        snapshot.exteriorCellX = coordinates->cellX;
                        snapshot.exteriorCellY = coordinates->cellY;
                    }
                }

                snapshot.displayName = !name.empty() ? name :
                    (!snapshot.containingLocation.empty() ? snapshot.containingLocation : editorID);
                snapshot.searchName = FoldTextForSearch(snapshot.displayName);
                snapshot.searchEditorID = FoldTextForSearch(snapshot.editorID);
                snapshot.searchPlugin = FoldTextForSearch(snapshot.SourcePlugin());
                snapshot.searchContext = FoldTextForSearch(snapshot.containingLocation);
                snapshot.searchWorldspace = FoldTextForSearch(snapshot.worldspace);
                rebuilt.push_back(std::move(snapshot));
            }
            return rebuilt;
        }
    }

    RuntimeIndex::RuntimeIndex() : RuntimeIndex(
        CatalogCapture{}, [] { return CaptureGameLocations(); })
    {}

    RuntimeIndex::RuntimeIndex(CatalogCapture catalogCapture) : RuntimeIndex(
        std::move(catalogCapture), [] {
            return std::expected<std::vector<LocationSnapshot>, IndexFailure>{
                std::vector<LocationSnapshot>{}};
        })
    {}

    RuntimeIndex::RuntimeIndex(
        CatalogCapture catalogCapture,
        LocationCatalogCapture locationCatalogCapture) :
        emptyCatalog_(std::make_shared<const std::vector<NpcSnapshot>>()),
        emptyLocationCatalog_(std::make_shared<const std::vector<LocationSnapshot>>()),
        published_(std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
            1, 1, IndexReadiness::Empty, IndexFailure::None,
            emptyCatalog_, emptyLocationCatalog_})),
        catalogCapture_(std::move(catalogCapture)),
        locationCatalogCapture_(std::move(locationCatalogCapture))
    {}

    std::uint64_t RuntimeIndex::NextRevision() noexcept
    {
        return nextRevision_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    RuntimeIndexView RuntimeIndex::Snapshot() const noexcept
    {
        const auto view = published_.load(std::memory_order_acquire);
        return IsCurrentIndexView(view, CurrentSession()) ? view : RuntimeIndexView{};
    }

    std::uint64_t RuntimeIndex::CurrentSession() const noexcept
    {
        return currentSession_.load(std::memory_order_acquire);
    }

    RuntimeIndexDiagnostics RuntimeIndex::Diagnostics() const noexcept
    {
        return {
            lastFullBuildMicroseconds_.load(std::memory_order_relaxed),
            targetedCloneCount_.load(std::memory_order_relaxed),
            noOpSuppressionCount_.load(std::memory_order_relaxed)};
    }

    std::uint64_t RuntimeIndex::AdvanceSession() noexcept
    {
        return currentSession_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    bool RuntimeIndex::PublishEmptyCurrentSession(std::uint64_t expectedSession) noexcept
    {
        try {
            std::scoped_lock lock(writerMutex_);
            if (expectedSession != CurrentSession()) return false;
            published_.store(
                std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
                    expectedSession,
                    NextRevision(),
                    IndexReadiness::Empty,
                    IndexFailure::None,
                    emptyCatalog_,
                    emptyLocationCatalog_}),
                std::memory_order_release);
            return true;
        } catch (...) {
            return false;
        }
    }

    void RuntimeIndex::BeginSession() noexcept
    {
        static_cast<void>(PublishEmptyCurrentSession(AdvanceSession()));
    }

    bool RuntimeIndex::PublishMetadata(
        std::uint64_t expectedSession,
        IndexReadiness readiness,
        IndexFailure failure) noexcept
    {
        try {
            std::scoped_lock lock(writerMutex_);
            if (expectedSession != CurrentSession()) return false;
            const auto prior = published_.load(std::memory_order_acquire);
            const auto catalog = prior && prior->session == expectedSession && prior->catalog ?
                prior->catalog : emptyCatalog_;
            const auto locations = prior && prior->session == expectedSession && prior->locations ?
                prior->locations : emptyLocationCatalog_;
            published_.store(
                std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
                    expectedSession, NextRevision(), readiness, failure, catalog, locations}),
                std::memory_order_release);
            return true;
        } catch (...) {
            return false;
        }
    }

    void RuntimeIndex::PublishFailureLocked(
        std::uint64_t expectedSession,
        IndexFailure failure)
    {
        if (expectedSession != CurrentSession()) return;
        const auto prior = published_.load(std::memory_order_acquire);
        const auto catalog = prior && prior->session == expectedSession && prior->catalog ?
            prior->catalog : emptyCatalog_;
        const auto locations = prior && prior->session == expectedSession && prior->locations ?
            prior->locations : emptyLocationCatalog_;
        published_.store(
            std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
                expectedSession, NextRevision(), IndexReadiness::Failed, failure, catalog, locations}),
            std::memory_order_release);
    }

    IndexFailure RuntimeIndex::Rebuild(
        std::uint64_t expectedSession,
        std::span<const std::uint32_t> trackedRuntimeFormIDs)
    {
        FullBuildTimer timer(lastFullBuildMicroseconds_);
        std::scoped_lock writerLock(writerMutex_);
        if (expectedSession != CurrentSession()) return IndexFailure::BuildFailed;

        auto captured = catalogCapture_ ? catalogCapture_() : CaptureGameCatalog(*this);
        if (!captured) {
            if (expectedSession != CurrentSession()) return IndexFailure::BuildFailed;
            PublishFailureLocked(expectedSession, captured.error());
            return captured.error();
        }
        auto rebuilt = std::move(*captured);

        auto capturedLocations = locationCatalogCapture_ ? locationCatalogCapture_() :
            std::expected<std::vector<LocationSnapshot>, IndexFailure>{
                std::vector<LocationSnapshot>{}};
        if (!capturedLocations) {
            if (expectedSession != CurrentSession()) return IndexFailure::BuildFailed;
            PublishFailureLocked(expectedSession, capturedLocations.error());
            return capturedLocations.error();
        }
        auto rebuiltLocations = std::move(*capturedLocations);

        ApplyTrackedState(rebuilt, trackedRuntimeFormIDs);
        if (expectedSession != CurrentSession()) return IndexFailure::BuildFailed;
        published_.store(
            MakeIndexCatalogView(
                expectedSession,
                NextRevision(),
                std::move(rebuilt),
                std::move(rebuiltLocations)),
            std::memory_order_release);
        return IndexFailure::None;
    }

    std::optional<NpcSnapshot> RuntimeIndex::FindSnapshot(std::uint32_t runtimeFormID) const
    {
        const auto view = Snapshot();
        if (!view) return std::nullopt;
        const auto found = std::ranges::find_if(*view->catalog, [&](const auto& snapshot) {
            return snapshot.ReferenceRuntimeID() == runtimeFormID;
        });
        return found == view->catalog->end() ? std::nullopt : std::optional<NpcSnapshot>{*found};
    }

    bool RuntimeIndex::ContainsRuntimeId(std::uint32_t runtimeFormID) const noexcept
    {
        const auto view = Snapshot();
        return view && std::ranges::any_of(*view->catalog, [&](const auto& snapshot) {
            return snapshot.ReferenceRuntimeID() == runtimeFormID;
        });
    }

    bool RuntimeIndex::IsPluginLoaded(std::string_view plugin) const
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler && (dataHandler->LookupLoadedModByName(plugin) != nullptr ||
            dataHandler->LookupLoadedLightModByName(plugin) != nullptr);
    }

    bool RuntimeIndex::RefreshRuntimeId(std::uint32_t runtimeFormID)
    {
        std::scoped_lock lock(writerMutex_);
        const auto view = Snapshot();
        if (!view) return false;
        const auto found = std::ranges::find_if(*view->catalog, [&](const auto& snapshot) {
            return snapshot.ReferenceRuntimeID() == runtimeFormID;
        });
        if (found == view->catalog->end()) return false;
        auto refreshed = *found;
        RefreshDynamic(refreshed);
        if (refreshed == *found) {
            noOpSuppressionCount_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        auto catalog = *view->catalog;
        catalog[static_cast<std::size_t>(found - view->catalog->begin())] = std::move(refreshed);
        published_.store(
            MakeIndexContentView(view, NextRevision(), std::move(catalog)),
            std::memory_order_release);
        targetedCloneCount_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool RuntimeIndex::SetExpectedEnabledState(std::uint32_t runtimeFormID, bool enabled)
    {
        std::scoped_lock lock(writerMutex_);
        const auto view = Snapshot();
        if (!view) return false;
        const auto found = std::ranges::find_if(*view->catalog, [&](const auto& snapshot) {
            return snapshot.ReferenceRuntimeID() == runtimeFormID;
        });
        if (found == view->catalog->end()) return false;
        if (found->enabled == enabled) {
            noOpSuppressionCount_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        auto catalog = *view->catalog;
        catalog[static_cast<std::size_t>(found - view->catalog->begin())].enabled = enabled;
        published_.store(
            MakeIndexContentView(view, NextRevision(), std::move(catalog)),
            std::memory_order_release);
        targetedCloneCount_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void RuntimeIndex::SetTrackedRuntimeIds(std::span<const std::uint32_t> runtimeFormIDs)
    {
        std::scoped_lock lock(writerMutex_);
        const auto view = Snapshot();
        if (!view) return;
        auto catalog = *view->catalog;
        ApplyTrackedState(catalog, runtimeFormIDs);
        if (catalog == *view->catalog) {
            noOpSuppressionCount_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        published_.store(
            MakeIndexContentView(view, NextRevision(), std::move(catalog)),
            std::memory_order_release);
        targetedCloneCount_.fetch_add(1, std::memory_order_relaxed);
    }

    bool RuntimeIndex::RefreshRuntimeAndTracking(
        std::uint32_t runtimeFormID,
        std::span<const std::uint32_t> trackedRuntimeFormIDs)
    {
        std::scoped_lock lock(writerMutex_);
        const auto view = Snapshot();
        if (!view) return false;
        auto catalog = *view->catalog;
        ApplyTrackedState(catalog, trackedRuntimeFormIDs);
        bool targetFound = runtimeFormID == 0;
        if (runtimeFormID != 0) {
            const auto found = std::ranges::find_if(catalog, [&](const auto& snapshot) {
                return snapshot.ReferenceRuntimeID() == runtimeFormID;
            });
            targetFound = found != catalog.end();
            if (targetFound) RefreshDynamic(*found);
        }
        if (catalog != *view->catalog) {
            published_.store(
                MakeIndexContentView(view, NextRevision(), std::move(catalog)),
                std::memory_order_release);
            targetedCloneCount_.fetch_add(1, std::memory_order_relaxed);
        } else {
            noOpSuppressionCount_.fetch_add(1, std::memory_order_relaxed);
        }
        return targetFound;
    }

    void RuntimeIndex::RefreshDynamic(NpcSnapshot& snapshot) const
    {
        const auto actor = ResolveRuntime(snapshot.ReferenceRuntimeID());
        if (!actor) {
            snapshot.alive = false;
            snapshot.enabled = false;
            snapshot.teammate = false;
            snapshot.potentialFollower = false;
            snapshot.loaded = false;
            snapshot.available = false;
            snapshot.essential = false;
            snapshot.protectedActor = false;
            snapshot.health = 0.0F;
            snapshot.magicka = 0.0F;
            snapshot.stamina = 0.0F;
            snapshot.race.clear();
            snapshot.sex.clear();
            snapshot.spatial.freshness =
                snapshot.spatial.location.empty() && snapshot.spatial.cell.empty() &&
                    snapshot.spatial.worldspace.empty() ?
                SpatialFreshness::Unavailable : SpatialFreshness::LastObserved;
            snapshot.spatial.sameCell = false;
            snapshot.spatial.sameLocation = false;
            snapshot.spatial.sameWorldspace = false;
            snapshot.spatial.movementBoundary = MovementBoundary::Unknown;
            snapshot.spatial.distance.reset();
            return;
        }

        snapshot.alive = !actor->IsDead();
        snapshot.enabled = !actor->IsDisabled();
        snapshot.teammate = actor->IsPlayerTeammate();
        const auto* defaultObjects = RE::BGSDefaultObjectManager::GetSingleton();
        const auto* potentialFollowerFaction = defaultObjects ?
            defaultObjects->GetObject<RE::TESFaction>(
                RE::DEFAULT_OBJECT::kPotentialFollowerFaction) : nullptr;
        snapshot.potentialFollower = potentialFollowerFaction &&
            actor->IsInFaction(potentialFollowerFaction);
        snapshot.loaded = actor->Is3DLoaded();
        snapshot.available = true;
        snapshot.essential = actor->IsEssential();
        snapshot.protectedActor = actor->IsProtected();
        if (auto* values = actor->AsActorValueOwner()) {
            snapshot.health = values->GetActorValue(RE::ActorValue::kHealth);
            snapshot.magicka = values->GetActorValue(RE::ActorValue::kMagicka);
            snapshot.stamina = values->GetActorValue(RE::ActorValue::kStamina);
        }
        snapshot.level = actor->GetLevel();
        if (auto editorID = CopyEditorID(actor.get()); !editorID.empty()) {
            snapshot.referenceEditorID = std::move(editorID);
        }
        snapshot.spatial = CaptureSpatial(*actor);
        snapshot.race = CopyFormName(actor->GetRace());
        if (const auto* base = actor->GetActorBase()) {
            if (auto editorID = CopyEditorID(base); !editorID.empty()) {
                snapshot.baseEditorID = std::move(editorID);
            }
            const auto sex = base->GetSex();
            snapshot.sex = sex == RE::SEX::kFemale ? "Female" :
                           sex == RE::SEX::kMale ? "Male" : "Unknown";
        } else {
            snapshot.sex = "Unknown";
        }
        RefreshSearchKeys(snapshot);
    }

    RE::NiPointer<RE::Actor> RuntimeIndex::Resolve(const FormIdentity& identity) const
    {
        if (!identity.IsPersistable()) return {};
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* actor = dataHandler ?
            dataHandler->LookupForm<RE::Actor>(identity.localID, identity.plugin) : nullptr;
        return IsSearchableActor(actor) ? actor->GetHandle().get() : RE::NiPointer<RE::Actor>{};
    }

    RE::NiPointer<RE::Actor> RuntimeIndex::ResolveRuntime(std::uint32_t runtimeFormID) const
    {
        if (runtimeFormID == 0) return {};
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(runtimeFormID);
        return IsSearchableActor(actor) ? actor->GetHandle().get() : RE::NiPointer<RE::Actor>{};
    }
}
