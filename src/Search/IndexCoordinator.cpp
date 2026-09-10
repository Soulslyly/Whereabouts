#include "PCH.h"

#include "Search/IndexCoordinator.h"
#include "Search/RuntimeIndex.h"

namespace whereabouts
{
    IndexCoordinator::IndexCoordinator(
        RuntimeIndex& index,
        IndexCoordinatorServices services) noexcept :
        index_(index),
        services_(std::move(services))
    {}

    IndexRequestResult IndexCoordinator::EnsureReady()
    {
        return Request(IndexRequestReason::FirstOpen, false);
    }

    IndexRequestResult IndexCoordinator::ForceRefresh(IndexRequestReason reason)
    {
        return Request(reason, true);
    }

    IndexRequestResult IndexCoordinator::Request(IndexRequestReason reason, bool force)
    {
        std::scoped_lock lock(requestMutex_);
        if (services_.beforeStateObservation) services_.beforeStateObservation();
        const auto session = index_.CurrentSession();
        const auto view = index_.Snapshot();
        const auto readiness = view ? view->readiness : IndexReadiness::Empty;
        if (requestActive_ && requestSession_ == session) {
            markerRepairRequested_ = MergeMarkerRepairIntent(
                markerRepairRequested_, reason);
            return IndexRequestResult::Coalesced;
        }
        if (DecideIndexRequest(readiness, force) == IndexRequestAction::AlreadyReady) {
            return IndexRequestResult::AlreadyReady;
        }
        requestActive_ = true;
        requestSession_ = session;
        markerRepairRequested_ = AllowsMarkerRepair(reason);
        if (!services_.queueTask) {
            static_cast<void>(index_.PublishMetadata(
                session, IndexReadiness::Failed, IndexFailure::TaskInterfaceUnavailable));
            requestActive_ = false;
            logger::error("Search index request failed: SKSE task interface unavailable");
            return IndexRequestResult::Failed;
        }

        static_cast<void>(index_.PublishMetadata(session, IndexReadiness::Queued));
        bool queued = false;
        try {
            queued = services_.queueTask([this, session] { ExecuteBuild(session); });
        } catch (...) {
            queued = false;
        }
        if (!queued) {
            static_cast<void>(index_.PublishMetadata(
                session, IndexReadiness::Failed, IndexFailure::TaskInterfaceUnavailable));
            requestActive_ = false;
            logger::error("Search index request failed: SKSE task scheduling failed");
            return IndexRequestResult::Failed;
        }
        logger::info("Search index queued for session {}", session);
        return IndexRequestResult::Queued;
    }

    bool IndexCoordinator::RebuildNow(IndexRequestReason reason)
    {
        std::uint64_t session = 0;
        {
            std::scoped_lock lock(requestMutex_);
            session = index_.CurrentSession();
            if (requestActive_ && requestSession_ == session) {
                markerRepairRequested_ = MergeMarkerRepairIntent(
                    markerRepairRequested_, reason);
                return false;
            }
            requestActive_ = true;
            requestSession_ = session;
            markerRepairRequested_ = AllowsMarkerRepair(reason);
        }
        ExecuteBuild(session);
        const auto view = index_.Snapshot();
        return view && view->readiness == IndexReadiness::Ready;
    }

    void IndexCoordinator::ExecuteBuild(std::uint64_t session) noexcept
    {
        try {
            {
                std::scoped_lock lock(requestMutex_);
                if (!requestActive_ || requestSession_ != session ||
                    session != index_.CurrentSession()) {
                    return;
                }
            }

            static_cast<void>(index_.PublishMetadata(session, IndexReadiness::Building));
            const auto trackedIds = services_.captureTrackedRuntimeIds ?
                services_.captureTrackedRuntimeIds() : std::vector<std::uint32_t>{};

            const auto failure = index_.Rebuild(session, trackedIds);
            const bool current = session == index_.CurrentSession();
            bool repairMarkers = false;
            {
                std::scoped_lock lock(requestMutex_);
                if (requestSession_ == session) {
                    repairMarkers = markerRepairRequested_;
                    requestActive_ = false;
                    markerRepairRequested_ = false;
                }
            }

            if (!current) {
                logger::info("Discarded stale Search index build for session {}", session);
                return;
            }
            if (failure != IndexFailure::None) {
                if (failure != IndexFailure::FormsUnavailable) {
                    static_cast<void>(index_.PublishMetadata(
                        session, IndexReadiness::Failed, failure));
                }
                logger::error("Search index build failed with code {}", static_cast<int>(failure));
                return;
            }

            if (repairMarkers && services_.repairMarkers) {
                const auto refreshed = services_.repairMarkers();
                if (!refreshed) {
                    logger::warn("Could not refresh tracking markers: {}", refreshed.error());
                }
            }
            const auto view = index_.Snapshot();
            const auto diagnostics = index_.Diagnostics();
            logger::info(
                "NPC and location indexes ready: session {}, revision {}, NPCs {}, locations {}, tracked {}",
                session,
                view ? view->revision : 0,
                view ? view->catalog->size() : 0,
                view ? view->locations->size() : 0,
                trackedIds.size());
            logger::debug(
                "Search index diagnostics: build {} us, targeted clones {}, no-op suppressions {}",
                diagnostics.lastFullBuildMicroseconds,
                diagnostics.targetedCloneCount,
                diagnostics.noOpSuppressionCount);
        } catch (const std::exception& error) {
            static_cast<void>(index_.PublishMetadata(
                session, IndexReadiness::Failed, IndexFailure::BuildFailed));
            {
                std::scoped_lock lock(requestMutex_);
                if (requestSession_ == session) requestActive_ = false;
            }
            logger::error("Search index build exception: {}", error.what());
        } catch (...) {
            static_cast<void>(index_.PublishMetadata(
                session, IndexReadiness::Failed, IndexFailure::BuildFailed));
            {
                std::scoped_lock lock(requestMutex_);
                if (requestSession_ == session) requestActive_ = false;
            }
            logger::error("Search index build failed with an unknown exception");
        }
    }

    void IndexCoordinator::BeginSessionBoundary(bool publishEmpty) noexcept
    {
        const auto session = index_.AdvanceSession();
        if (publishEmpty) static_cast<void>(index_.PublishEmptyCurrentSession(session));
        try {
            std::scoped_lock lock(requestMutex_);
            requestActive_ = false;
            markerRepairRequested_ = false;
            requestSession_ = session;
            // A requester that already owned requestMutex_ can observe the new
            // session and publish Queued before this boundary acquires the lock.
            // Reset the public view while the request state is being invalidated
            // so that a boundary never leaves that abandoned request visible.
            if (publishEmpty) static_cast<void>(index_.PublishEmptyCurrentSession(session));
        } catch (...) {
            // The index session was already invalidated before entering this lock.
        }
    }
}
