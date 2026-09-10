#pragma once

#include "Core/FormIdentity.h"
#include "Core/LocationSnapshot.h"
#include "Core/NpcSnapshot.h"
#include "Core/RuntimeIndexSnapshot.h"

#include <RE/Skyrim.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <atomic>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace whereabouts
{
    struct RuntimeIndexDiagnostics
    {
        std::uint64_t lastFullBuildMicroseconds{0};
        std::uint64_t targetedCloneCount{0};
        std::uint64_t noOpSuppressionCount{0};
    };

    class RuntimeIndex
    {
    public:
        using CatalogCapture = std::function<
            std::expected<std::vector<NpcSnapshot>, IndexFailure>()>;
        using LocationCatalogCapture = std::function<
            std::expected<std::vector<LocationSnapshot>, IndexFailure>()>;

        RuntimeIndex();
        explicit RuntimeIndex(CatalogCapture catalogCapture);
        RuntimeIndex(
            CatalogCapture catalogCapture,
            LocationCatalogCapture locationCatalogCapture);

        [[nodiscard]] IndexFailure Rebuild(
            std::uint64_t expectedSession,
            std::span<const std::uint32_t> trackedRuntimeFormIDs = {});
        [[nodiscard]] RuntimeIndexView Snapshot() const noexcept;
        [[nodiscard]] std::uint64_t CurrentSession() const noexcept;
        [[nodiscard]] RuntimeIndexDiagnostics Diagnostics() const noexcept;
        [[nodiscard]] std::uint64_t AdvanceSession() noexcept;
        [[nodiscard]] bool PublishEmptyCurrentSession(std::uint64_t expectedSession) noexcept;
        void BeginSession() noexcept;
        [[nodiscard]] bool PublishMetadata(
            std::uint64_t expectedSession,
            IndexReadiness readiness,
            IndexFailure failure = IndexFailure::None) noexcept;
        [[nodiscard]] std::optional<NpcSnapshot> FindSnapshot(std::uint32_t runtimeFormID) const;
        [[nodiscard]] bool ContainsRuntimeId(std::uint32_t runtimeFormID) const noexcept;
        [[nodiscard]] bool IsPluginLoaded(std::string_view plugin) const;
        [[nodiscard]] bool RefreshRuntimeId(std::uint32_t runtimeFormID);
        [[nodiscard]] bool SetExpectedEnabledState(
            std::uint32_t runtimeFormID,
            bool enabled);
        [[nodiscard]] bool RefreshRuntimeAndTracking(
            std::uint32_t runtimeFormID,
            std::span<const std::uint32_t> trackedRuntimeFormIDs);
        void RefreshDynamic(NpcSnapshot& snapshot) const;
        void SetTrackedRuntimeIds(std::span<const std::uint32_t> runtimeFormIDs);

        [[nodiscard]] RE::NiPointer<RE::Actor> Resolve(const FormIdentity& identity) const;
        [[nodiscard]] RE::NiPointer<RE::Actor> ResolveRuntime(std::uint32_t runtimeFormID) const;

    private:
        [[nodiscard]] std::uint64_t NextRevision() noexcept;
        void PublishFailureLocked(std::uint64_t expectedSession, IndexFailure failure);

        std::shared_ptr<const std::vector<NpcSnapshot>> emptyCatalog_;
        std::shared_ptr<const std::vector<LocationSnapshot>> emptyLocationCatalog_;
        std::atomic<RuntimeIndexView> published_;
        std::atomic_uint64_t currentSession_{1};
        std::atomic_uint64_t nextRevision_{1};
        std::atomic_uint64_t lastFullBuildMicroseconds_{0};
        std::atomic_uint64_t targetedCloneCount_{0};
        std::atomic_uint64_t noOpSuppressionCount_{0};
        CatalogCapture catalogCapture_;
        LocationCatalogCapture locationCatalogCapture_;
        mutable std::mutex writerMutex_;
    };
}
