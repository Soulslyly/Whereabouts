#pragma once

#include "Core/FormIdentity.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <expected>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace whereabouts
{
    inline constexpr std::uint32_t kSavedNpcSchemaVersion = 1;
    inline constexpr std::size_t kMaximumSavedNpcEntries = 4096;
    inline constexpr std::size_t kMaximumPluginNameBytes = 260;
    inline constexpr std::size_t kMaximumLastKnownNameBytes = 1024;
    inline constexpr std::uint32_t kTrackedDeathSchemaVersion = 1;
    inline constexpr std::size_t kMaximumTrackedDeathEntries = 1024;
    inline constexpr std::size_t kMaximumLastKnownLocationBytes = 1024;
    inline constexpr std::uint16_t kMaximumTrackedObjectives = 100;
    inline constexpr std::uint16_t kNoTrackedObjective = 0xFFFF;

    struct SavedNpcEntry
    {
        FormIdentity identity;
        std::string lastKnownName;

        [[nodiscard]] bool operator==(const SavedNpcEntry&) const noexcept = default;
    };

    struct SerializationError
    {
        std::string message;
    };

    enum class TrackedDeathState : std::uint8_t
    {
        BodyPresent,
        BodyMissing
    };

    struct TrackedDeathEntry
    {
        std::uint32_t runtimeFormID{0};
        std::optional<FormIdentity> identity;
        std::uint16_t objectiveIndex{kNoTrackedObjective};
        TrackedDeathState state{TrackedDeathState::BodyPresent};
        std::string lastKnownName;
        std::string lastKnownLocation;

        [[nodiscard]] bool operator==(const TrackedDeathEntry&) const noexcept = default;
    };

    struct SavedNpcStoreSnapshot
    {
        std::vector<SavedNpcEntry> favorites;
        std::vector<SavedNpcEntry> recent;
        std::vector<TrackedDeathEntry> trackedDeaths;
        bool trackingWarningAcknowledged{false};

        [[nodiscard]] bool operator==(const SavedNpcStoreSnapshot&) const noexcept = default;
    };

    [[nodiscard]] bool HasSerializableState(const SavedNpcStoreSnapshot& snapshot) noexcept;

    using EncodedSavedNpcEntries = std::expected<std::vector<std::byte>, SerializationError>;
    using DecodedSavedNpcEntries = std::expected<std::vector<SavedNpcEntry>, SerializationError>;
    using EncodedTrackedDeathEntries = std::expected<std::vector<std::byte>, SerializationError>;
    using DecodedTrackedDeathEntries = std::expected<std::vector<TrackedDeathEntry>, SerializationError>;

    [[nodiscard]] EncodedSavedNpcEntries EncodeSavedNpcEntries(
        std::span<const SavedNpcEntry> entries);
    [[nodiscard]] DecodedSavedNpcEntries DecodeSavedNpcEntries(
        std::span<const std::byte> bytes);
    [[nodiscard]] EncodedTrackedDeathEntries EncodeTrackedDeathEntries(
        std::span<const TrackedDeathEntry> entries);
    [[nodiscard]] DecodedTrackedDeathEntries DecodeTrackedDeathEntries(
        std::span<const std::byte> bytes);
    [[nodiscard]] std::array<std::byte, 1> EncodeTrackingWarningAcknowledgement(bool acknowledged) noexcept;
    [[nodiscard]] std::expected<bool, SerializationError> DecodeTrackingWarningAcknowledgement(
        std::span<const std::byte> bytes);

    class SavedNpcStore
    {
    public:
        [[nodiscard]] bool AddFavorite(SavedNpcEntry entry);
        [[nodiscard]] bool RemoveFavorite(const FormIdentity& identity);
        [[nodiscard]] std::size_t RemoveFavorites(std::span<const FormIdentity> identities);
        [[nodiscard]] bool RecordRecent(SavedNpcEntry entry);
        [[nodiscard]] bool RemoveRecent(const FormIdentity& identity);
        [[nodiscard]] std::size_t RemoveRecent(std::span<const FormIdentity> identities);
        void SetRecentLimit(std::size_t limit);
        void ReplaceFavorites(std::span<const SavedNpcEntry> entries);
        void ReplaceRecent(std::span<const SavedNpcEntry> entries);
        [[nodiscard]] bool UpsertTrackedDeath(TrackedDeathEntry entry);
        [[nodiscard]] bool RemoveTrackedDeath(std::uint32_t runtimeFormID);
        [[nodiscard]] bool RemoveTrackedDeath(const TrackedDeathEntry& identity);
        void ReplaceTrackedDeaths(std::span<const TrackedDeathEntry> entries);
        void ClearFavorites();
        void ClearRecent();
        void ClearTrackedDeaths();
        void Clear();
        void ReplaceAll(const SavedNpcStoreSnapshot& snapshot);

        [[nodiscard]] SavedNpcStoreSnapshot SnapshotAll() const;
        [[nodiscard]] std::vector<SavedNpcEntry> Favorites() const;
        [[nodiscard]] std::vector<SavedNpcEntry> Recent() const;
        [[nodiscard]] std::vector<TrackedDeathEntry> TrackedDeaths() const;
        [[nodiscard]] bool TrackingWarningAcknowledged() const;
        void AcknowledgeTrackingWarning();

    private:
        void TrimRecent();

        std::vector<SavedNpcEntry> favorites_;
        std::vector<SavedNpcEntry> recent_;
        std::vector<TrackedDeathEntry> trackedDeaths_;
        std::size_t recentLimit_{20};
        bool trackingWarningAcknowledged_{false};
        mutable std::mutex mutex_;
    };
}
