#include "PCH.h"

#include "Commands/CommandPolicy.h"
#include "Core/FormIdentityAdapter.h"
#include "Core/SpatialPresentation.h"
#include "Core/SnapshotState.h"
#include "Core/TextFold.h"
#include "Search/EditorIdLookup.h"
#include "Search/PluginStructure.h"
#include "Search/RuntimeIndex.h"
#include "Targets/TargetSelection.h"

#include <chrono>
#include <expected>
#include <limits>
#include <string_view>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef GetObject
#undef GetObject
#endif

namespace whereabouts
{
    std::vector<PlacedNpcRecordCandidate> SelectWinningPlacedNpcRecords(
        std::span<const PlacedNpcRecordCandidate> candidates)
    {
        std::unordered_map<std::uint32_t, PlacedNpcRecordCandidate> winners;
        for (const auto& candidate : candidates) {
            if (candidate.referenceRuntimeFormID == 0) continue;
            if (!candidate.deleted &&
                (candidate.baseRuntimeFormID == 0 || !candidate.referenceStable.IsPersistable())) {
                continue;
            }
            winners.insert_or_assign(candidate.referenceRuntimeFormID, candidate);
        }

        std::vector<PlacedNpcRecordCandidate> selected;
        selected.reserve(winners.size());
        for (auto& [formID, candidate] : winners) {
            if (!candidate.deleted) selected.push_back(std::move(candidate));
        }
        std::ranges::sort(selected, {}, &PlacedNpcRecordCandidate::referenceRuntimeFormID);
        return selected;
    }

    std::vector<std::uint32_t> MergeActorDiscoveryCandidateIds(
        std::span<const std::uint32_t> actorArrayIds,
        std::span<const std::uint32_t> globalRegistryIds,
        std::span<const std::uint32_t> cellPersistentIds,
        std::span<const std::uint32_t> cellActiveIds)
    {
        std::vector<std::uint32_t> merged;
        std::unordered_set<std::uint32_t> seen;
        merged.reserve(
            actorArrayIds.size() + globalRegistryIds.size() + cellPersistentIds.size() +
            cellActiveIds.size());
        const auto append = [&](std::span<const std::uint32_t> source) {
            for (const auto formID : source) {
                if (formID != 0 && seen.insert(formID).second) merged.push_back(formID);
            }
        };
        append(actorArrayIds);
        append(globalRegistryIds);
        append(cellPersistentIds);
        append(cellActiveIds);
        return merged;
    }

    namespace
    {
        constexpr std::uint32_t kDeletedRecordFlag = 1U << 5;
        constexpr std::uint32_t kNameSubrecord = 0x454D414E;  // NAME

        [[nodiscard]] const RE::TESFile* RawFormOwner(
            const RE::TESFile& source,
            std::uint32_t rawFormID) noexcept
        {
            const auto masterIndex = rawFormID >> 24;
            if (masterIndex < source.masterCount && source.masterPtrs) {
                if (const auto* owner = source.masterPtrs[masterIndex]) return owner;
            }
            return std::addressof(source);
        }

        [[nodiscard]] FormIdentity StableIdentityFromRawFormID(
            const RE::TESFile& source,
            std::uint32_t rawFormID,
            std::uint32_t runtimeFormID)
        {
            const auto* owner = RawFormOwner(source, rawFormID);
            return owner ? FormIdentity::FromRuntimeFormID(
                std::string(owner->GetFilename()), runtimeFormID, owner->IsLight()) : FormIdentity{};
        }

        bool ScanPlacedNpcRecords(
            RE::TESFile& source,
            std::vector<PlacedNpcRecordCandidate>& candidates,
            std::size_t& structureFiles,
            std::size_t& structureOpenFailures,
            std::size_t& structureSeekFailures,
            std::size_t& structureReadFailures,
            std::size_t& structureParserFailures,
            std::size_t& mappedReferences)
        {
            ++structureFiles;
            std::unordered_map<std::uint32_t, std::uint32_t> cellParents;
            RE::BSResourceNiBinaryStream structureStream{std::string(source.GetFilename())};
            if (!structureStream.good() || !structureStream.stream) {
                ++structureOpenFailures;
            } else {
                enum class StreamFailure
                {
                    None,
                    Seek,
                    Read
                };
                StreamFailure streamFailure = StreamFailure::None;
                const auto parents = ScanPlacedRecordCellParents(
                    structureStream.stream->totalSize,
                    [&structureStream, &streamFailure](
                        std::uint64_t offset,
                        std::span<std::byte> output) {
                        std::uint64_t sought = 0;
                        if (structureStream.stream->DoSeek(
                                offset, RE::BSResource::SeekMode::kSet, sought) !=
                                RE::BSResource::ErrorCode::kNone || sought != offset) {
                            streamFailure = StreamFailure::Seek;
                            return false;
                        }
                        std::uint64_t bytesRead = 0;
                        if (structureStream.stream->DoRead(
                                output.data(), output.size(), bytesRead) !=
                                RE::BSResource::ErrorCode::kNone || bytesRead != output.size()) {
                            streamFailure = StreamFailure::Read;
                            return false;
                        }
                        return true;
                    });
                if (parents) {
                    mappedReferences += parents->size();
                    cellParents.reserve(parents->size());
                    for (const auto& parent : *parents) {
                        cellParents.insert_or_assign(
                            parent.rawReferenceFormID, parent.rawCellFormID);
                    }
                } else if (streamFailure == StreamFailure::Seek) {
                    ++structureSeekFailures;
                } else if (streamFailure == StreamFailure::Read) {
                    ++structureReadFailures;
                } else {
                    ++structureParserFailures;
                }
            }

            auto* reader = source.Duplicate();
            if (!reader) return false;
            struct ReaderCloser
            {
                RE::TESFile* value;
                ~ReaderCloser() { if (value) static_cast<void>(value->CloseTES(true)); }
            } closer{reader};
            if (!reader->OpenTES(RE::NiFile::OpenMode::kReadOnly, false)) {
                return false;
            }

            while (reader->SeekNextForm(false)) {
                if (reader->GetFormType() != RE::FormType::ActorCharacter) continue;

                const auto rawReferenceFormID = reader->currentform.formID;
                const auto referenceRuntimeFormID = source.GetRuntimeFormID(rawReferenceFormID);
                if (referenceRuntimeFormID == 0 || referenceRuntimeFormID == 0x14) continue;

                PlacedNpcRecordCandidate candidate;
                candidate.referenceRuntimeFormID = referenceRuntimeFormID;
                candidate.referenceStable = StableIdentityFromRawFormID(
                    source, rawReferenceFormID, referenceRuntimeFormID);
                candidate.deleted = (reader->currentform.flags & kDeletedRecordFlag) != 0;
                if (!candidate.deleted) {
                    if (const auto parent = cellParents.find(rawReferenceFormID);
                        parent != cellParents.end()) {
                        const auto cellRuntimeFormID = source.GetRuntimeFormID(parent->second);
                        const auto identity = StableIdentityFromRawFormID(
                            source, parent->second, cellRuntimeFormID);
                        if (cellRuntimeFormID != 0 && identity.IsPersistable()) {
                            candidate.recordedCell = RecordedCellSnapshot{
                                .identity = identity,
                                .runtimeFormID = cellRuntimeFormID};
                        }
                    }
                    std::uint32_t rawBaseFormID = 0;
                    if (!reader->SeekNextSubrecordType(kNameSubrecord) ||
                        reader->GetCurrentSubRecordSize() < sizeof(rawBaseFormID) ||
                        !reader->ReadData(std::addressof(rawBaseFormID), sizeof(rawBaseFormID))) {
                        continue;
                    }
                    candidate.baseRuntimeFormID = source.GetRuntimeFormID(rawBaseFormID);
                }
                candidates.push_back(std::move(candidate));
            }
            return true;
        }

        std::vector<PlacedNpcRecordCandidate> CapturePlacedNpcRecords(
            RE::TESDataHandler& dataHandler)
        {
            static std::mutex fileScanMutex;
            std::scoped_lock lock(fileScanMutex);

            std::vector<PlacedNpcRecordCandidate> candidates;
            std::size_t scannedFiles = 0;
            std::size_t failedFiles = 0;
            std::size_t structureFiles = 0;
            std::size_t structureOpenFailures = 0;
            std::size_t structureSeekFailures = 0;
            std::size_t structureReadFailures = 0;
            std::size_t structureParserFailures = 0;
            std::size_t mappedReferences = 0;
            const auto scan = [&](RE::TESFile* file) {
                if (!file) return;
                if (ScanPlacedNpcRecords(
                        *file,
                        candidates,
                        structureFiles,
                        structureOpenFailures,
                        structureSeekFailures,
                        structureReadFailures,
                        structureParserFailures,
                        mappedReferences)) ++scannedFiles;
                else ++failedFiles;
            };

            std::unordered_set<RE::TESFile*> loadedFiles;
            auto** regular = dataHandler.GetLoadedMods();
            for (std::uint16_t i = 0; regular && i < dataHandler.GetLoadedModCount(); ++i) {
                if (regular[i]) loadedFiles.insert(regular[i]);
            }
            auto** light = dataHandler.GetLoadedLightMods();
            for (std::uint16_t i = 0; light && i < dataHandler.GetLoadedLightModCount(); ++i) {
                if (light[i]) loadedFiles.insert(light[i]);
            }

            // TESDataHandler::files retains the mixed full/light load order that
            // decides record winners. The compiled arrays above are only used as
            // an active-file allowlist because each array loses that interleaving.
            std::size_t orderedFiles = 0;
            for (auto* file : dataHandler.files) {
                if (file && loadedFiles.erase(file) != 0) {
                    scan(file);
                    ++orderedFiles;
                }
            }
            // Fail closed on an incomplete file-list view without dropping an
            // otherwise active plugin from discovery.
            if (!loadedFiles.empty()) {
                for (std::uint16_t i = 0; regular && i < dataHandler.GetLoadedModCount(); ++i) {
                    if (regular[i] && loadedFiles.erase(regular[i]) != 0) scan(regular[i]);
                }
                for (std::uint16_t i = 0; light && i < dataHandler.GetLoadedLightModCount(); ++i) {
                    if (light[i] && loadedFiles.erase(light[i]) != 0) scan(light[i]);
                }
            }

            auto winning = SelectWinningPlacedNpcRecords(candidates);
            const auto winningWithCells = static_cast<std::size_t>(std::ranges::count_if(
                winning, [](const auto& candidate) { return candidate.recordedCell.has_value(); }));
            logger::info(
                "Placed NPC record discovery: ordered files {}, files {}, failed {}, records {}, winning {}",
                orderedFiles,
                scannedFiles,
                failedFiles,
                candidates.size(),
                winning.size());
            logger::info(
                "Placed record cell hierarchy: files {}, resource open {}, seek {}, read {}, parser {}, mapped references {}, winning rows with cells {}",
                structureFiles,
                structureOpenFailures,
                structureSeekFailures,
                structureReadFailures,
                structureParserFailures,
                mappedReferences,
                winningWithCells);
            return winning;
        }

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

        [[nodiscard]] bool HasSpatialEvidence(const SpatialSnapshot& spatial) noexcept
        {
            return !spatial.location.empty() || !spatial.cell.empty() ||
                !spatial.worldspace.empty() || spatial.locationFormID != 0 ||
                spatial.cellFormID != 0 || spatial.worldspaceFormID != 0;
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
            spatial.freshness = ClassifySpatialFreshness(
                HasSpatialEvidence(spatial), actor.Is3DLoaded());
            return spatial;
        }

        void RefreshSearchKeys(NpcSnapshot& snapshot)
        {
            snapshot.searchKeys.name = FoldTextForSearch(snapshot.displayName);
            snapshot.searchKeys.editorIDs = FoldTextForSearch(
                snapshot.referenceEditorID + " " + snapshot.baseEditorID);
            snapshot.searchKeys.plugin = FoldTextForSearch(snapshot.SourcePlugin());
            snapshot.searchKeys.location = FoldTextForSearch(SearchableSpatialText(snapshot.spatial));
            snapshot.searchKeys.race = FoldTextForSearch(snapshot.race);
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
                if (type != RE::FormType::ActorCharacter && type != RE::FormType::NPC &&
                    type != RE::FormType::Faction && type != RE::FormType::Keyword) {
                    continue;
                }
                result.Observe(form->GetFormID(), editorID.data());
            }
            return result;
        }

        [[nodiscard]] RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG TemplateFlag(
            TemplateDataCategory category) noexcept
        {
            switch (category) {
            case TemplateDataCategory::Traits:
                return RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kTraits;
            case TemplateDataCategory::Stats:
                return RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kStats;
            case TemplateDataCategory::Factions:
                return RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kFactions;
            case TemplateDataCategory::AIData:
                return RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kAIData;
            case TemplateDataCategory::BaseData:
                return RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kBaseData;
            case TemplateDataCategory::Keywords:
                return RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kKeywords;
            }
            return RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kNone;
        }

        [[nodiscard]] TemplateOwnerResult<RE::TESNPC> ResolveNpcTemplateOwner(
            const RE::TESNPC* base,
            TemplateDataCategory category)
        {
            return ResolveTemplateOwner(
                base,
                category,
                [](const RE::TESNPC& npc) { return npc.GetFormID(); },
                [](const RE::TESNPC& npc, TemplateDataCategory requested) {
                    return npc.actorData.templateUseFlags.any(TemplateFlag(requested));
                },
                [](const RE::TESNPC& npc) {
                    const auto* form = npc.baseTemplateForm;
                    return TemplateLink<RE::TESNPC>{
                        form ? form->As<RE::TESNPC>() : nullptr,
                        form != nullptr};
                });
        }

        [[nodiscard]] RecordFacet CaptureRecordFacet(
            const RE::TESForm* form,
            std::string displayName,
            const EditorIdLookup& editorIds)
        {
            RecordFacet facet;
            if (!form) return facet;
            facet.runtimeFormID = form->GetFormID();
            if (const auto identity = TryGetFormIdentity(form)) facet.identity = *identity;
            facet.editorID = editorIds.Find(facet.runtimeFormID);
            if (facet.editorID.empty()) facet.editorID = CopyEditorID(form);
            facet.label = !displayName.empty() ? std::move(displayName) : facet.editorID;
            if (facet.label.empty()) facet.label = std::format("{:08X}", facet.runtimeFormID);
            facet.searchText = FoldTextForSearch(
                facet.label + " " + facet.editorID + " " + facet.identity.plugin);
            return facet;
        }

        [[nodiscard]] NpcRecordProjection CaptureRecordProjection(
            const RE::TESNPC* base,
            const EditorIdLookup& editorIds)
        {
            NpcRecordProjection projection;
            if (!base) return projection;

            if (const auto* files = base->sourceFiles.array) {
                std::vector<std::string> sources;
                sources.reserve(files->size());
                for (const auto* file : *files) {
                    if (file && !file->GetFilename().empty()) {
                        sources.emplace_back(file->GetFilename());
                    }
                }
                projection.provenance = BuildRecordProvenance(sources);
            }

            const auto baseData = ResolveNpcTemplateOwner(base, TemplateDataCategory::BaseData);
            if (baseData.state == TemplateResolutionState::Known && baseData.owner) {
                projection.baseDataOwnerRuntimeFormID = baseData.owner->GetFormID();
                projection.actorFlags = {
                    .known = true,
                    .essential = baseData.owner->IsEssential(),
                    .protectedActor = baseData.owner->IsProtected()};
            }

            const auto traits = ResolveNpcTemplateOwner(base, TemplateDataCategory::Traits);
            if (traits.state == TemplateResolutionState::Known && traits.owner) {
                projection.traitsOwnerRuntimeFormID = traits.owner->GetFormID();
                projection.traitsKnown = true;
                projection.race = CopyFormName(traits.owner->race);
                projection.voiceTypeKnown = true;
                projection.voiceType = CaptureRecordFacet(
                    traits.owner->voiceType, {}, editorIds);
                switch (traits.owner->GetSex()) {
                case RE::SEX::kMale:
                    projection.sex = NpcSex::Male;
                    break;
                case RE::SEX::kFemale:
                    projection.sex = NpcSex::Female;
                    break;
                default:
                    projection.sex = NpcSex::Unknown;
                    break;
                }
            }

            const auto stats = ResolveNpcTemplateOwner(base, TemplateDataCategory::Stats);
            if (stats.state == TemplateResolutionState::Known && stats.owner) {
                projection.statsOwnerRuntimeFormID = stats.owner->GetFormID();
                projection.classKnown = true;
                projection.npcClass = CaptureRecordFacet(
                    stats.owner->npcClass, CopyFormName(stats.owner->npcClass), editorIds);
                projection.levelScaling = {
                    .known = true,
                    .playerLevelMult = stats.owner->HasPCLevelMult(),
                    .value = stats.owner->actorData.level,
                    .minimum = stats.owner->actorData.calcLevelMin,
                    .maximum = stats.owner->actorData.calcLevelMax};
            }

            const auto aiData = ResolveNpcTemplateOwner(base, TemplateDataCategory::AIData);
            if (aiData.state == TemplateResolutionState::Known && aiData.owner) {
                projection.aiDataOwnerRuntimeFormID = aiData.owner->GetFormID();
                projection.combatStyleKnown = true;
                projection.combatStyle = CaptureRecordFacet(
                    aiData.owner->combatStyle,
                    {},
                    editorIds);
            }

            const auto factions = ResolveNpcTemplateOwner(base, TemplateDataCategory::Factions);
            if (factions.state == TemplateResolutionState::Known && factions.owner) {
                projection.factionsKnown = true;
                std::unordered_set<std::uint32_t> observed;
                for (const auto& member : factions.owner->factions) {
                    if (!member.faction || member.rank < 0) continue;
                    const auto id = member.faction->GetFormID();
                    if (id == 0 || !observed.insert(id).second) continue;
                    if (projection.factions.size() == kMaximumRecordFacetsPerCategory) {
                        projection.factionsKnown = false;
                        projection.factions.clear();
                        break;
                    }
                    projection.factions.push_back(CaptureRecordFacet(
                        member.faction, CopyFormName(member.faction), editorIds));
                }
            }

            const auto keywords = ResolveNpcTemplateOwner(base, TemplateDataCategory::Keywords);
            if (keywords.state == TemplateResolutionState::Known && keywords.owner) {
                projection.keywordsKnown = true;
                std::unordered_set<std::uint32_t> observed;
                for (const auto* keyword : keywords.owner->GetKeywords()) {
                    if (!keyword) continue;
                    const auto id = keyword->GetFormID();
                    if (id == 0 || !observed.insert(id).second) continue;
                    if (projection.keywords.size() == kMaximumRecordFacetsPerCategory) {
                        projection.keywordsKnown = false;
                        projection.keywords.clear();
                        break;
                    }
                    projection.keywords.push_back(CaptureRecordFacet(
                        keyword, {}, editorIds));
                }
            }

            return projection;
        }

        using RecordProjectionCache = std::unordered_map<
            std::uint32_t,
            std::shared_ptr<const NpcRecordProjection>>;

        [[nodiscard]] std::optional<NpcSnapshot> CaptureActorSnapshot(
            RuntimeIndex& index,
            RE::Actor* actor,
            const EditorIdLookup& editorIds,
            RecordProjectionCache& recordProjectionCache)
        {
            if (!IsSearchableActor(actor)) return std::nullopt;

            const auto* base = actor->GetActorBase();
            const auto* referenceName = actor->GetDisplayFullName();
            const auto displayName = PreferredNpcDisplayName(
                referenceName ? std::string_view(referenceName) : std::string_view{},
                CopyDisplayName(base));
            if (displayName.empty()) return std::nullopt;

            NpcSnapshot snapshot;
            snapshot.identity.reference.runtimeFormID = actor->GetFormID();
            snapshot.identity.base.runtimeFormID = base->GetFormID();
            snapshot.identity.uniqueBase = base->IsUnique();
            snapshot.displayName = displayName;
            if (const auto identity = TryGetFormIdentity(actor)) {
                snapshot.identity.reference.stable = *identity;
            }
            if (!snapshot.identity.IsPersistable()) return std::nullopt;
            if (const auto identity = TryGetFormIdentity(base)) {
                snapshot.identity.base.stable = *identity;
            }
            snapshot.referenceEditorID = editorIds.Find(snapshot.identity.reference.runtimeFormID);
            snapshot.baseEditorID = editorIds.Find(snapshot.identity.base.runtimeFormID);
            snapshot.recordProjection = GetOrCreateRecordProjection(
                recordProjectionCache,
                base->GetFormID(),
                [&] { return CaptureRecordProjection(base, editorIds); });
            index.RefreshDynamic(snapshot);
            return snapshot;
        }

        [[nodiscard]] std::optional<NpcSnapshot> CapturePlacedNpcSnapshot(
            RuntimeIndex& index,
            const PlacedNpcRecordCandidate& record,
            const EditorIdLookup& editorIds,
            RecordProjectionCache& recordProjectionCache)
        {
            auto* base = RE::TESForm::LookupByID<RE::TESNPC>(record.baseRuntimeFormID);
            if (!base || base->IsDeleted()) return std::nullopt;
            const auto displayName = CopyDisplayName(base);
            if (displayName.empty()) return std::nullopt;

            NpcSnapshot snapshot;
            snapshot.identity.reference.runtimeFormID = record.referenceRuntimeFormID;
            snapshot.identity.reference.stable = record.referenceStable;
            snapshot.identity.base.runtimeFormID = base->GetFormID();
            snapshot.identity.uniqueBase = base->IsUnique();
            snapshot.displayName = displayName;
            if (const auto identity = TryGetFormIdentity(base)) {
                snapshot.identity.base.stable = *identity;
            }
            if (!snapshot.identity.IsPersistable()) return std::nullopt;
            snapshot.referenceEditorID = editorIds.Find(record.referenceRuntimeFormID);
            snapshot.baseEditorID = editorIds.Find(base->GetFormID());
            snapshot.recordProjection = GetOrCreateRecordProjection(
                recordProjectionCache,
                base->GetFormID(),
                [&] { return CaptureRecordProjection(base, editorIds); });
            if (record.recordedCell) {
                snapshot.recordedCell = *record.recordedCell;
                auto* cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(
                    record.recordedCell->runtimeFormID);
                const auto identity = TryGetFormIdentity(cell);
                if (cell && identity && *identity == record.recordedCell->identity) {
                    snapshot.recordedCell->displayName = CopyDisplayName(cell);
                    snapshot.recordedCell->editorID = editorIds.Find(cell->GetFormID());
                    snapshot.recordedCell->interior = cell->IsInteriorCell();
                    snapshot.recordedCell->worldspaceName =
                        CopyDisplayName(cell->GetRuntimeData().worldSpace);
                }
            }
            index.RefreshDynamic(snapshot);
            return snapshot;
        }

        [[nodiscard]] std::optional<NpcSnapshot> CaptureRuntimeSnapshot(
            RuntimeIndex& index,
            std::uint32_t runtimeFormID)
        {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(runtimeFormID);
            const auto editorIds = CaptureEditorIds();
            RecordProjectionCache recordProjectionCache;
            return CaptureActorSnapshot(index, actor, editorIds, recordProjectionCache);
        }

        std::expected<std::vector<NpcSnapshot>, IndexFailure> CaptureGameCatalog(
            RuntimeIndex& index)
        {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) return std::unexpected(IndexFailure::FormsUnavailable);

            const auto editorIds = CaptureEditorIds();
            const auto placedRecords = CapturePlacedNpcRecords(*dataHandler);
            std::unordered_map<std::uint32_t, RE::NiPointer<RE::Actor>> actorPointers;

            std::vector<std::uint32_t> actorArrayIds;
            const auto& actors = dataHandler->GetFormArray<RE::Actor>();
            actorArrayIds.reserve(actors.size());
            for (auto* actor : actors) {
                if (!actor) continue;
                const auto formID = actor->GetFormID();
                actorArrayIds.push_back(formID);
                actorPointers.try_emplace(formID, RE::NiPointer<RE::Actor>{actor});
            }

            std::vector<std::uint32_t> globalRegistryIds;
            {
                const auto& [forms, lock] = RE::TESForm::GetAllForms();
                const RE::BSReadLockGuard guard{lock};
                if (forms) {
                    globalRegistryIds.reserve(forms->size());
                    for (const auto& [formID, form] : *forms) {
                        if (form && form->GetFormType() == RE::FormType::ActorCharacter) {
                            globalRegistryIds.push_back(formID);
                            if (auto* actor = form->As<RE::Actor>()) {
                                actorPointers.try_emplace(
                                    formID, RE::NiPointer<RE::Actor>{actor});
                            }
                        }
                    }
                }
            }

            std::vector<std::uint32_t> cellPersistentIds;
            std::vector<std::uint32_t> cellActiveIds;
            const auto& cells = dataHandler->GetFormArray<RE::TESObjectCELL>();
            for (auto* cell : cells) {
                if (!cell) continue;
                auto& runtimeData = cell->GetRuntimeData();
                const RE::BSSpinLockGuard guard{runtimeData.spinLock};
                for (auto* reference : runtimeData.objectList) {
                    if (reference &&
                        reference->GetFormType() == RE::FormType::ActorCharacter) {
                        const auto formID = reference->GetFormID();
                        cellPersistentIds.push_back(formID);
                        if (auto* actor = reference->As<RE::Actor>()) {
                            actorPointers.try_emplace(
                                formID, RE::NiPointer<RE::Actor>{actor});
                        }
                    }
                }
                for (const auto& reference : runtimeData.references) {
                    if (reference &&
                        reference->GetFormType() == RE::FormType::ActorCharacter) {
                        const auto formID = reference->GetFormID();
                        cellActiveIds.push_back(formID);
                        if (auto* actor = reference->As<RE::Actor>()) {
                            actorPointers.try_emplace(
                                formID, RE::NiPointer<RE::Actor>{actor});
                        }
                    }
                }
            }

            const auto candidateIDs = MergeActorDiscoveryCandidateIds(
                actorArrayIds,
                globalRegistryIds,
                cellPersistentIds,
                cellActiveIds);

            std::vector<NpcSnapshot> rebuilt;
            rebuilt.reserve(placedRecords.size() + candidateIDs.size());
            RecordProjectionCache recordProjectionCache;
            std::unordered_set<std::uint32_t> placedReferenceIDs;
            placedReferenceIDs.reserve(placedRecords.size());
            std::size_t recordBackedRows = 0;
            for (const auto& record : placedRecords) {
                const auto found = actorPointers.find(record.referenceRuntimeFormID);
                auto* actor = found != actorPointers.end() ? found->second.get() :
                    RE::TESForm::LookupByID<RE::Actor>(record.referenceRuntimeFormID);
                auto snapshot = CaptureActorSnapshot(
                    index, actor, editorIds, recordProjectionCache);
                bool recordBacked = false;
                if (!snapshot) {
                    snapshot = CapturePlacedNpcSnapshot(
                        index, record, editorIds, recordProjectionCache);
                    recordBacked = snapshot.has_value();
                }
                if (snapshot) {
                    placedReferenceIDs.insert(record.referenceRuntimeFormID);
                    if (recordBacked) ++recordBackedRows;
                    rebuilt.push_back(std::move(*snapshot));
                }
            }
            for (const auto formID : candidateIDs) {
                if (placedReferenceIDs.contains(formID)) continue;
                const auto found = actorPointers.find(formID);
                auto* actor = found != actorPointers.end() ?
                    found->second.get() : RE::TESForm::LookupByID<RE::Actor>(formID);
                if (auto snapshot = CaptureActorSnapshot(
                        index, actor, editorIds, recordProjectionCache)) {
                    rebuilt.push_back(std::move(*snapshot));
                }
            }
            logger::info(
                "Actor discovery: placed {}, record-only {}, actor array {}, global registry {}, cell-persistent {}, cell-active {}, live unique {}, indexed {}",
                placedRecords.size(),
                recordBackedRows,
                actorArrayIds.size(),
                globalRegistryIds.size(),
                cellPersistentIds.size(),
                cellActiveIds.size(),
                candidateIDs.size(),
                rebuilt.size());
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
        LocationCatalogCapture locationCatalogCapture) : RuntimeIndex(
            std::move(catalogCapture),
            std::move(locationCatalogCapture),
            RuntimeRowCapture{})
    {}

    RuntimeIndex::RuntimeIndex(
        CatalogCapture catalogCapture,
        LocationCatalogCapture locationCatalogCapture,
        RuntimeRowCapture runtimeRowCapture) :
        emptyCatalog_(std::make_shared<const std::vector<NpcSnapshot>>()),
        emptyLocationCatalog_(std::make_shared<const std::vector<LocationSnapshot>>()),
        published_(std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
            1, 1, IndexReadiness::Empty, IndexFailure::None,
            emptyCatalog_, emptyLocationCatalog_})),
        catalogCapture_(std::move(catalogCapture)),
        locationCatalogCapture_(std::move(locationCatalogCapture)),
        runtimeRowCapture_(std::move(runtimeRowCapture))
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
            trackedRuntimeFormIDs_.clear();
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
        ApplyIndexedReferenceCounts(rebuilt);

        auto capturedLocations = locationCatalogCapture_ ? locationCatalogCapture_() :
            std::expected<std::vector<LocationSnapshot>, IndexFailure>{
                std::vector<LocationSnapshot>{}};
        if (!capturedLocations) {
            if (expectedSession != CurrentSession()) return IndexFailure::BuildFailed;
            PublishFailureLocked(expectedSession, capturedLocations.error());
            return capturedLocations.error();
        }
        auto rebuiltLocations = std::move(*capturedLocations);

        trackedRuntimeFormIDs_.assign(
            trackedRuntimeFormIDs.begin(), trackedRuntimeFormIDs.end());
        ApplyTrackedState(rebuilt, trackedRuntimeFormIDs_);
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
        if (found == view->catalog->end()) {
            auto captured = runtimeRowCapture_ ?
                runtimeRowCapture_(runtimeFormID) : CaptureRuntimeSnapshot(*this, runtimeFormID);
            if (!captured || captured->ReferenceRuntimeID() != runtimeFormID) return false;
            captured->tracked = std::ranges::find(
                trackedRuntimeFormIDs_, runtimeFormID) != trackedRuntimeFormIDs_.end();
            auto catalog = *view->catalog;
            catalog.push_back(std::move(*captured));
            ApplyIndexedReferenceCounts(catalog);
            published_.store(
                MakeIndexContentView(view, NextRevision(), std::move(catalog)),
                std::memory_order_release);
            targetedCloneCount_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
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

    bool RuntimeIndex::SetExpectedActorFlagsForOwner(
        std::uint32_t ownerRuntimeFormID,
        bool essential,
        bool protectedActor)
    {
        if (ownerRuntimeFormID == 0) return false;
        std::scoped_lock lock(writerMutex_);
        const auto view = Snapshot();
        if (!view) return false;
        auto catalog = *view->catalog;
        bool found = false;
        bool changed = false;
        std::shared_ptr<const NpcRecordProjection> replacement;
        for (auto& row : catalog) {
            if (!row.recordProjection ||
                row.recordProjection->baseDataOwnerRuntimeFormID != ownerRuntimeFormID) {
                continue;
            }
            found = true;
            if (!replacement) {
                auto projection = *row.recordProjection;
                projection.actorFlags = {true, essential, protectedActor};
                replacement = std::make_shared<const NpcRecordProjection>(std::move(projection));
            }
            if (!row.actorFlagsKnown || row.essential != essential ||
                row.protectedActor != protectedActor || row.recordProjection != replacement) {
                row.actorFlagsKnown = true;
                row.essential = essential;
                row.protectedActor = protectedActor;
                row.recordProjection = replacement;
                changed = true;
            }
        }
        if (!found) return false;
        if (!changed) {
            noOpSuppressionCount_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        published_.store(
            MakeIndexContentView(view, NextRevision(), std::move(catalog)),
            std::memory_order_release);
        targetedCloneCount_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void RuntimeIndex::SetTrackedRuntimeIds(std::span<const std::uint32_t> runtimeFormIDs)
    {
        std::scoped_lock lock(writerMutex_);
        trackedRuntimeFormIDs_.assign(runtimeFormIDs.begin(), runtimeFormIDs.end());
        const auto view = Snapshot();
        if (!view) return;
        auto catalog = *view->catalog;
        ApplyTrackedState(catalog, trackedRuntimeFormIDs_);
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
        trackedRuntimeFormIDs_.assign(
            trackedRuntimeFormIDs.begin(), trackedRuntimeFormIDs.end());
        const auto view = Snapshot();
        if (!view) return false;
        auto catalog = *view->catalog;
        ApplyTrackedState(catalog, trackedRuntimeFormIDs_);
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
            const auto flags = SelectActorFlagObservation(
                {}, snapshot.recordProjection ? snapshot.recordProjection->actorFlags : ActorFlagObservation{});
            snapshot.actorFlagsKnown = flags.known;
            snapshot.essential = flags.essential;
            snapshot.protectedActor = flags.protectedActor;
            snapshot.health = 0.0F;
            snapshot.magicka = 0.0F;
            snapshot.stamina = 0.0F;
            snapshot.race = snapshot.recordProjection && snapshot.recordProjection->traitsKnown ?
                snapshot.recordProjection->race : std::string{};
            snapshot.sex = snapshot.recordProjection && snapshot.recordProjection->traitsKnown ?
                snapshot.recordProjection->sex : NpcSex::Unknown;
            snapshot.spatial.freshness = ClassifySpatialFreshness(
                HasSpatialEvidence(snapshot.spatial), false);
            snapshot.spatial.sameCell = false;
            snapshot.spatial.sameLocation = false;
            snapshot.spatial.sameWorldspace = false;
            snapshot.spatial.movementBoundary = MovementBoundary::Unknown;
            snapshot.spatial.distance.reset();
            RefreshSearchKeys(snapshot);
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
        const auto flags = SelectActorFlagObservation(
            ActorFlagObservation{
                .known = snapshot.loaded,
                .essential = snapshot.loaded && actor->IsEssential(),
                .protectedActor = snapshot.loaded && actor->IsProtected()},
            snapshot.recordProjection ? snapshot.recordProjection->actorFlags : ActorFlagObservation{});
        snapshot.actorFlagsKnown = flags.known;
        snapshot.essential = flags.essential;
        snapshot.protectedActor = flags.protectedActor;
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
        if (snapshot.race.empty() && snapshot.recordProjection &&
            snapshot.recordProjection->traitsKnown) {
            snapshot.race = snapshot.recordProjection->race;
        }
        if (const auto* base = actor->GetActorBase()) {
            if (auto editorID = CopyEditorID(base); !editorID.empty()) {
                snapshot.baseEditorID = std::move(editorID);
            }
            if (snapshot.recordProjection && snapshot.recordProjection->traitsKnown) {
                snapshot.sex = snapshot.recordProjection->sex;
            } else {
                const auto sex = base->GetSex();
                switch (sex) {
                case RE::SEX::kMale:
                    snapshot.sex = NpcSex::Male;
                    break;
                case RE::SEX::kFemale:
                    snapshot.sex = NpcSex::Female;
                    break;
                default:
                    snapshot.sex = NpcSex::Unknown;
                    break;
                }
            }
        } else {
            snapshot.sex = NpcSex::Unknown;
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
