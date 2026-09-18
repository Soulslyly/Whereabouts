#pragma once

#include "Persistence/Serialization.h"
#include "Persistence/SharedFavorites.h"

#include <expected>
#include <mutex>
#include <span>
#include <string>

namespace whereabouts
{
    class FavoriteService
    {
    public:
        FavoriteService(SavedNpcStore& store, SharedFavoritesRepository& repository);

        [[nodiscard]] bool SharingEnabled() const noexcept;
        [[nodiscard]] std::expected<void, std::string> SetSharingEnabled(bool enabled);
        [[nodiscard]] std::expected<void, std::string> MergeSharedForSession();
        [[nodiscard]] std::expected<bool, std::string> Toggle(SavedNpcEntry entry);
        [[nodiscard]] std::expected<void, std::string> Remove(const FormIdentity& identity);
        [[nodiscard]] std::expected<std::size_t, std::string> RemoveMany(
            std::span<const FormIdentity> identities);
        [[nodiscard]] std::expected<void, std::string> Clear();
        [[nodiscard]] std::expected<void, std::string> ClearForUninstall();

    private:
        [[nodiscard]] std::expected<void, std::string> CommitShared(
            std::span<const SavedNpcEntry> entries);
        [[nodiscard]] static std::vector<SavedNpcEntry> Merge(
            std::span<const SavedNpcEntry> shared,
            std::span<const SavedNpcEntry> current);

        SavedNpcStore& store_;
        SharedFavoritesRepository& repository_;
        mutable std::mutex mutex_;
        bool sharingEnabled_{false};
    };
}
