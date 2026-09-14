#pragma once

#include "Core/FormIdentity.h"
#include "Core/NpcSex.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace whereabouts
{
    inline constexpr std::size_t kMaximumTemplateDepth = 32;
    inline constexpr std::size_t kMaximumRecordFacetsPerCategory = 512;

    enum class TemplateDataCategory : std::uint16_t
    {
        Traits = 1U << 0U,
        Factions = 1U << 2U,
        BaseData = 1U << 7U,
        Keywords = 1U << 12U
    };

    struct ActorFlagObservation
    {
        bool known{false};
        bool essential{false};
        bool protectedActor{false};

        [[nodiscard]] bool operator==(const ActorFlagObservation&) const noexcept = default;
    };

    [[nodiscard]] constexpr ActorFlagObservation SelectActorFlagObservation(
        ActorFlagObservation live,
        ActorFlagObservation record) noexcept
    {
        return live.known ? live : record;
    }

    enum class TemplateResolutionState
    {
        Known,
        MissingTemplate,
        NonNpcTemplate,
        Cycle,
        DepthExceeded
    };

    template <class Node>
    struct TemplateLink
    {
        const Node* npc{nullptr};
        bool hasTemplate{false};
    };

    template <class Node>
    struct TemplateOwnerResult
    {
        const Node* owner{nullptr};
        TemplateResolutionState state{TemplateResolutionState::MissingTemplate};
    };

    template <class Node, class GetId, class InheritsCategory, class GetTemplate>
    [[nodiscard]] TemplateOwnerResult<Node> ResolveTemplateOwner(
        const Node* start,
        TemplateDataCategory category,
        GetId&& getId,
        InheritsCategory&& inheritsCategory,
        GetTemplate&& getTemplate)
    {
        if (!start) return {};

        std::array<std::uint32_t, kMaximumTemplateDepth> visited{};
        std::size_t visitedCount = 0;
        auto* current = start;
        while (visitedCount < visited.size()) {
            const auto id = static_cast<std::uint32_t>(getId(*current));
            for (std::size_t index = 0; index < visitedCount; ++index) {
                if (visited[index] == id) {
                    return {nullptr, TemplateResolutionState::Cycle};
                }
            }
            visited[visitedCount++] = id;

            if (!inheritsCategory(*current, category)) {
                return {current, TemplateResolutionState::Known};
            }

            const auto link = getTemplate(*current);
            if (!link.hasTemplate) {
                return {nullptr, TemplateResolutionState::MissingTemplate};
            }
            if (!link.npc) {
                return {nullptr, TemplateResolutionState::NonNpcTemplate};
            }
            current = link.npc;
        }
        return {nullptr, TemplateResolutionState::DepthExceeded};
    }

    struct RecordFacet
    {
        std::uint32_t runtimeFormID{0};
        FormIdentity identity;
        std::string label;
        std::string editorID;
        std::string searchText;

        [[nodiscard]] bool operator==(const RecordFacet&) const noexcept = default;
    };

    struct NpcRecordProjection
    {
        ActorFlagObservation actorFlags;
        bool traitsKnown{false};
        NpcSex sex{NpcSex::Unknown};
        std::string race;
        bool factionsKnown{false};
        std::vector<RecordFacet> factions;
        bool keywordsKnown{false};
        std::vector<RecordFacet> keywords;

        [[nodiscard]] bool operator==(const NpcRecordProjection&) const noexcept = default;
    };

    enum class RecordFacetCategory
    {
        Faction,
        Keyword
    };

    struct RecordFilterSelection
    {
        std::uint32_t runtimeFormID{0};
        FormIdentity identity;

        [[nodiscard]] bool operator==(const RecordFilterSelection&) const noexcept = default;
    };

    [[nodiscard]] inline bool IsRecordFacetKnown(
        const NpcRecordProjection& projection,
        RecordFacetCategory category) noexcept
    {
        return category == RecordFacetCategory::Faction ?
            projection.factionsKnown : projection.keywordsKnown;
    }

    [[nodiscard]] inline bool MatchesRecordFacet(
        const NpcRecordProjection& projection,
        RecordFacetCategory category,
        std::uint32_t runtimeFormID) noexcept
    {
        const auto& facets = category == RecordFacetCategory::Faction ?
            projection.factions : projection.keywords;
        for (const auto& facet : facets) {
            if (facet.runtimeFormID == runtimeFormID) return true;
        }
        return false;
    }

    template <class Factory>
    [[nodiscard]] std::shared_ptr<const NpcRecordProjection> GetOrCreateRecordProjection(
        std::unordered_map<std::uint32_t, std::shared_ptr<const NpcRecordProjection>>& cache,
        std::uint32_t baseRuntimeFormID,
        Factory&& factory)
    {
        if (const auto found = cache.find(baseRuntimeFormID); found != cache.end()) {
            return found->second;
        }
        auto projection = std::make_shared<const NpcRecordProjection>(factory());
        cache.emplace(baseRuntimeFormID, projection);
        return projection;
    }
}
