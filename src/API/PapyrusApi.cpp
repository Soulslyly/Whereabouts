#include "PCH.h"

#include "API/PapyrusApi.h"

#include "API/PublicApi.h"
#include "AppContext.h"
#include "Lifecycle/CallbackGuard.h"
#include "Lifecycle/ProcessContext.h"

#include <vector>

namespace whereabouts
{
    namespace
    {
        void LogPublicApiException() noexcept
        {
            try { logger::error("Contained exception in a public Papyrus API query"); }
            catch (...) {}
        }

        std::optional<PublicNpcRecord> RecordFor(RE::TESForm* form, bool includeFavorite = false)
        {
            auto* context = AcquireProcessContext();
            auto* actor = form ? form->As<RE::Actor>() : nullptr;
            if (!context || !actor) return std::nullopt;
            std::vector<FormIdentity> favoriteIdentities;
            if (includeFavorite) {
                const auto favorites = context->savedNpcs.Favorites();
                favoriteIdentities.reserve(favorites.size());
                for (const auto& favorite : favorites) {
                    favoriteIdentities.push_back(favorite.identity);
                }
            }
            return QueryPublicNpc(
                context->index.Snapshot(),
                context->index.CurrentSession(),
                context->runtimeReady.load(std::memory_order_acquire),
                actor->GetFormID(),
                favoriteIdentities);
        }

        enum class FacetKind
        {
            Faction,
            BaseKeyword
        };

        std::optional<RecordFacet> FacetFor(
            RE::TESForm* form,
            std::int32_t index,
            FacetKind kind)
        {
            if (index < 0) return std::nullopt;
            const auto record = RecordFor(form);
            if (!record || !record->recordProjection) return std::nullopt;
            const bool known = kind == FacetKind::Faction ?
                record->factionsKnown : record->baseKeywordsKnown;
            if (!known) return std::nullopt;
            const auto& facets = kind == FacetKind::Faction ?
                record->recordProjection->factions : record->recordProjection->keywords;
            const auto safeIndex = static_cast<std::size_t>(index);
            return safeIndex < facets.size() ?
                std::optional<RecordFacet>{facets[safeIndex]} : std::nullopt;
        }

        std::int32_t GetVersion(RE::StaticFunctionTag*) { return kPublicApiVersion; }
        bool SupportsVersion(RE::StaticFunctionTag*, std::int32_t version)
        {
            return SupportsPublicApiVersion(version);
        }
        bool IsReady(RE::StaticFunctionTag*)
        {
            return GuardCallback([] {
                auto* context = AcquireProcessContext();
                return context && IsPublicApiReady(
                    context->index.Snapshot(),
                    context->index.CurrentSession(),
                    context->runtimeReady.load(std::memory_order_acquire));
            }, false, LogPublicApiException);
        }
        bool HasNPC(RE::StaticFunctionTag*, RE::TESForm* form)
        {
            return GuardCallback([form] { return RecordFor(form).has_value(); }, false, LogPublicApiException);
        }

#define WHEREABOUTS_STRING_QUERY(functionName, fieldName) \
        std::string functionName(RE::StaticFunctionTag*, RE::TESForm* form) \
        { \
            return GuardCallback([form] { \
                const auto record = RecordFor(form); \
                return record ? record->fieldName : std::string{}; \
            }, std::string{}, LogPublicApiException); \
        }

#define WHEREABOUTS_BOOL_QUERY(functionName, fieldName) \
        bool functionName(RE::StaticFunctionTag*, RE::TESForm* form) \
        { \
            return GuardCallback([form] { \
                const auto record = RecordFor(form); \
                return record && record->fieldName; \
            }, false, LogPublicApiException); \
        }

#define WHEREABOUTS_INT_QUERY(functionName, fieldName, fallbackValue) \
        std::int32_t functionName(RE::StaticFunctionTag*, RE::TESForm* form) \
        { \
            return GuardCallback([form] { \
                const auto record = RecordFor(form); \
                return record ? static_cast<std::int32_t>(record->fieldName) : fallbackValue; \
            }, fallbackValue, LogPublicApiException); \
        }

        WHEREABOUTS_STRING_QUERY(GetName, name)
        WHEREABOUTS_STRING_QUERY(GetStableReferenceID, stableReferenceID)
        WHEREABOUTS_STRING_QUERY(GetReferenceEditorID, referenceEditorID)
        WHEREABOUTS_STRING_QUERY(GetBaseEditorID, baseEditorID)
        WHEREABOUTS_STRING_QUERY(GetLocation, location)
        WHEREABOUTS_STRING_QUERY(GetCell, cell)
        WHEREABOUTS_STRING_QUERY(GetWorldspace, worldspace)
        std::int32_t GetLocationStatus(RE::StaticFunctionTag*, RE::TESForm* form)
        {
            return GuardCallback([form] {
                const auto record = RecordFor(form);
                return record ? static_cast<std::int32_t>(record->locationStatus) :
                    static_cast<std::int32_t>(PublicLocationStatus::Unavailable);
            }, static_cast<std::int32_t>(PublicLocationStatus::Unavailable), LogPublicApiException);
        }
        WHEREABOUTS_BOOL_QUERY(IsAlive, alive)
        WHEREABOUTS_BOOL_QUERY(IsEnabled, enabled)
        WHEREABOUTS_BOOL_QUERY(IsLoaded, loaded)
        WHEREABOUTS_BOOL_QUERY(IsFollower, follower)
        WHEREABOUTS_BOOL_QUERY(IsPotentialFollower, potentialFollower)
        WHEREABOUTS_BOOL_QUERY(IsTracked, tracked)
        WHEREABOUTS_BOOL_QUERY(IsGeneric, generic)
        WHEREABOUTS_BOOL_QUERY(HasTraitData, traitsKnown)
        WHEREABOUTS_STRING_QUERY(GetRace, race)
        WHEREABOUTS_INT_QUERY(GetSex, sex, static_cast<std::int32_t>(PublicNpcSex::Unknown))
        WHEREABOUTS_BOOL_QUERY(HasActorFlagData, actorFlagsKnown)
        WHEREABOUTS_BOOL_QUERY(IsEssential, essential)
        WHEREABOUTS_BOOL_QUERY(IsProtected, protectedActor)
        WHEREABOUTS_INT_QUERY(GetAreaType, areaType, static_cast<std::int32_t>(PublicAreaType::Unknown))
        WHEREABOUTS_STRING_QUERY(GetWorldspaceFormID, worldspaceFormID)
        WHEREABOUTS_BOOL_QUERY(HasFactionData, factionsKnown)
        WHEREABOUTS_BOOL_QUERY(HasBaseKeywordData, baseKeywordsKnown)

        std::int32_t GetFactionCount(RE::StaticFunctionTag*, RE::TESForm* form)
        {
            return GuardCallback([form] {
                const auto record = RecordFor(form);
                return record && record->factionsKnown && record->recordProjection ?
                    static_cast<std::int32_t>(record->recordProjection->factions.size()) : 0;
            }, 0, LogPublicApiException);
        }
        std::int32_t GetBaseKeywordCount(RE::StaticFunctionTag*, RE::TESForm* form)
        {
            return GuardCallback([form] {
                const auto record = RecordFor(form);
                return record && record->baseKeywordsKnown && record->recordProjection ?
                    static_cast<std::int32_t>(record->recordProjection->keywords.size()) : 0;
            }, 0, LogPublicApiException);
        }

#define WHEREABOUTS_FACET_STRING_QUERY(functionName, kindName, valueExpression) \
        std::string functionName( \
            RE::StaticFunctionTag*, RE::TESForm* form, std::int32_t index) \
        { \
            return GuardCallback([form, index] { \
                const auto facet = FacetFor(form, index, FacetKind::kindName); \
                return facet ? (valueExpression) : std::string{}; \
            }, std::string{}, LogPublicApiException); \
        }

        WHEREABOUTS_FACET_STRING_QUERY(
            GetFactionFormID, Faction, FormatPublicRuntimeFormID(facet->runtimeFormID))
        WHEREABOUTS_FACET_STRING_QUERY(
            GetFactionStableID, Faction, FormatPublicStableID(facet->identity))
        WHEREABOUTS_FACET_STRING_QUERY(GetFactionName, Faction, facet->label)
        WHEREABOUTS_FACET_STRING_QUERY(GetFactionEditorID, Faction, facet->editorID)
        WHEREABOUTS_FACET_STRING_QUERY(
            GetBaseKeywordFormID, BaseKeyword, FormatPublicRuntimeFormID(facet->runtimeFormID))
        WHEREABOUTS_FACET_STRING_QUERY(
            GetBaseKeywordStableID, BaseKeyword, FormatPublicStableID(facet->identity))
        WHEREABOUTS_FACET_STRING_QUERY(GetBaseKeywordName, BaseKeyword, facet->label)
        WHEREABOUTS_FACET_STRING_QUERY(GetBaseKeywordEditorID, BaseKeyword, facet->editorID)

        bool IsFavorite(RE::StaticFunctionTag*, RE::TESForm* form)
        {
            return GuardCallback([form] {
                const auto record = RecordFor(form, true);
                return record && record->favorite;
            }, false, LogPublicApiException);
        }

#undef WHEREABOUTS_BOOL_QUERY
#undef WHEREABOUTS_INT_QUERY
#undef WHEREABOUTS_STRING_QUERY
#undef WHEREABOUTS_FACET_STRING_QUERY
    }

    bool RegisterPublicPapyrus(RE::BSScript::IVirtualMachine* vm)
    {
        if (!vm) return false;
        vm->RegisterFunction("GetVersion", "WhereaboutsAPI", GetVersion);
        vm->RegisterFunction("SupportsVersion", "WhereaboutsAPI", SupportsVersion);
        vm->RegisterFunction("IsReady", "WhereaboutsAPI", IsReady);
        vm->RegisterFunction("HasNPC", "WhereaboutsAPI", HasNPC);
        vm->RegisterFunction("GetName", "WhereaboutsAPI", GetName);
        vm->RegisterFunction("GetStableReferenceID", "WhereaboutsAPI", GetStableReferenceID);
        vm->RegisterFunction("GetReferenceEditorID", "WhereaboutsAPI", GetReferenceEditorID);
        vm->RegisterFunction("GetBaseEditorID", "WhereaboutsAPI", GetBaseEditorID);
        vm->RegisterFunction("GetLocation", "WhereaboutsAPI", GetLocation);
        vm->RegisterFunction("GetCell", "WhereaboutsAPI", GetCell);
        vm->RegisterFunction("GetWorldspace", "WhereaboutsAPI", GetWorldspace);
        vm->RegisterFunction("GetLocationStatus", "WhereaboutsAPI", GetLocationStatus);
        vm->RegisterFunction("IsAlive", "WhereaboutsAPI", IsAlive);
        vm->RegisterFunction("IsEnabled", "WhereaboutsAPI", IsEnabled);
        vm->RegisterFunction("IsLoaded", "WhereaboutsAPI", IsLoaded);
        vm->RegisterFunction("IsFollower", "WhereaboutsAPI", IsFollower);
        vm->RegisterFunction("IsPotentialFollower", "WhereaboutsAPI", IsPotentialFollower);
        vm->RegisterFunction("IsTracked", "WhereaboutsAPI", IsTracked);
        vm->RegisterFunction("IsFavorite", "WhereaboutsAPI", IsFavorite);
        vm->RegisterFunction("IsGeneric", "WhereaboutsAPI", IsGeneric);
        vm->RegisterFunction("HasTraitData", "WhereaboutsAPI", HasTraitData);
        vm->RegisterFunction("GetRace", "WhereaboutsAPI", GetRace);
        vm->RegisterFunction("GetSex", "WhereaboutsAPI", GetSex);
        vm->RegisterFunction("HasActorFlagData", "WhereaboutsAPI", HasActorFlagData);
        vm->RegisterFunction("IsEssential", "WhereaboutsAPI", IsEssential);
        vm->RegisterFunction("IsProtected", "WhereaboutsAPI", IsProtected);
        vm->RegisterFunction("GetAreaType", "WhereaboutsAPI", GetAreaType);
        vm->RegisterFunction("GetWorldspaceFormID", "WhereaboutsAPI", GetWorldspaceFormID);
        vm->RegisterFunction("HasFactionData", "WhereaboutsAPI", HasFactionData);
        vm->RegisterFunction("GetFactionCount", "WhereaboutsAPI", GetFactionCount);
        vm->RegisterFunction("GetFactionFormID", "WhereaboutsAPI", GetFactionFormID);
        vm->RegisterFunction("GetFactionStableID", "WhereaboutsAPI", GetFactionStableID);
        vm->RegisterFunction("GetFactionName", "WhereaboutsAPI", GetFactionName);
        vm->RegisterFunction("GetFactionEditorID", "WhereaboutsAPI", GetFactionEditorID);
        vm->RegisterFunction("HasBaseKeywordData", "WhereaboutsAPI", HasBaseKeywordData);
        vm->RegisterFunction("GetBaseKeywordCount", "WhereaboutsAPI", GetBaseKeywordCount);
        vm->RegisterFunction("GetBaseKeywordFormID", "WhereaboutsAPI", GetBaseKeywordFormID);
        vm->RegisterFunction("GetBaseKeywordStableID", "WhereaboutsAPI", GetBaseKeywordStableID);
        vm->RegisterFunction("GetBaseKeywordName", "WhereaboutsAPI", GetBaseKeywordName);
        vm->RegisterFunction("GetBaseKeywordEditorID", "WhereaboutsAPI", GetBaseKeywordEditorID);
        return true;
    }
}
