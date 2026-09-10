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

        std::int32_t GetVersion(RE::StaticFunctionTag*) { return kPublicApiVersion; }
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

        bool IsFavorite(RE::StaticFunctionTag*, RE::TESForm* form)
        {
            return GuardCallback([form] {
                const auto record = RecordFor(form, true);
                return record && record->favorite;
            }, false, LogPublicApiException);
        }

#undef WHEREABOUTS_BOOL_QUERY
#undef WHEREABOUTS_STRING_QUERY
    }

    bool RegisterPublicPapyrus(RE::BSScript::IVirtualMachine* vm)
    {
        if (!vm) return false;
        vm->RegisterFunction("GetVersion", "WhereaboutsAPI", GetVersion);
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
        return true;
    }
}
