#pragma once

#include "Core/NpcSnapshot.h"
#include "Core/LocationSnapshot.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace whereabouts
{
    enum class IndexReadiness
    {
        Empty,
        Queued,
        Building,
        Ready,
        Failed
    };

    enum class IndexFailure
    {
        None,
        TaskInterfaceUnavailable,
        FormsUnavailable,
        BuildFailed
    };

    struct RuntimeIndexSnapshot
    {
        std::uint64_t session{0};
        std::uint64_t revision{0};
        IndexReadiness readiness{IndexReadiness::Empty};
        IndexFailure failure{IndexFailure::None};
        std::shared_ptr<const std::vector<NpcSnapshot>> catalog;
        std::shared_ptr<const std::vector<LocationSnapshot>> locations;
    };

    using RuntimeIndexView = std::shared_ptr<const RuntimeIndexSnapshot>;

    [[nodiscard]] inline RuntimeIndexView MakeInitialIndexView(
        std::uint64_t session,
        std::uint64_t revision)
    {
        auto empty = std::make_shared<const std::vector<NpcSnapshot>>();
        auto emptyLocations = std::make_shared<const std::vector<LocationSnapshot>>();
        return std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
            session, revision, IndexReadiness::Empty, IndexFailure::None,
            std::move(empty), std::move(emptyLocations)});
    }

    [[nodiscard]] inline RuntimeIndexView MakeIndexMetadataView(
        const RuntimeIndexView& prior,
        std::uint64_t revision,
        IndexReadiness readiness,
        IndexFailure failure)
    {
        const auto catalog = prior && prior->catalog ?
            prior->catalog : std::make_shared<const std::vector<NpcSnapshot>>();
        const auto locations = prior && prior->locations ?
            prior->locations : std::make_shared<const std::vector<LocationSnapshot>>();
        return std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
            prior ? prior->session : 0, revision, readiness, failure, catalog, locations});
    }

    [[nodiscard]] inline RuntimeIndexView MakeIndexCatalogView(
        std::uint64_t session,
        std::uint64_t revision,
        std::vector<NpcSnapshot> catalog,
        std::vector<LocationSnapshot> locations)
    {
        auto owned = std::make_shared<const std::vector<NpcSnapshot>>(std::move(catalog));
        auto ownedLocations = std::make_shared<const std::vector<LocationSnapshot>>(std::move(locations));
        return std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
            session, revision, IndexReadiness::Ready, IndexFailure::None,
            std::move(owned), std::move(ownedLocations)});
    }

    [[nodiscard]] inline RuntimeIndexView MakeIndexCatalogView(
        std::uint64_t session,
        std::uint64_t revision,
        std::vector<NpcSnapshot> catalog)
    {
        return MakeIndexCatalogView(session, revision, std::move(catalog), {});
    }

    [[nodiscard]] inline RuntimeIndexView MakeIndexContentView(
        const RuntimeIndexView& prior,
        std::uint64_t revision,
        std::vector<NpcSnapshot> catalog)
    {
        auto owned = std::make_shared<const std::vector<NpcSnapshot>>(std::move(catalog));
        return std::make_shared<const RuntimeIndexSnapshot>(RuntimeIndexSnapshot{
            prior ? prior->session : 0,
            revision,
            prior ? prior->readiness : IndexReadiness::Empty,
            prior ? prior->failure : IndexFailure::None,
            std::move(owned),
            prior && prior->locations ? prior->locations :
                std::make_shared<const std::vector<LocationSnapshot>>()});
    }

    [[nodiscard]] inline bool IsCurrentIndexView(
        const RuntimeIndexView& view,
        std::uint64_t currentSession) noexcept
    {
        return view && view->catalog && view->locations && view->session == currentSession;
    }
}
