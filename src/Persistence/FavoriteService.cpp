#include "Persistence/FavoriteService.h"

#include <algorithm>
#include <array>
#include <utility>

namespace whereabouts
{
    FavoriteService::FavoriteService(
        SavedNpcStore& store,
        SharedFavoritesRepository& repository) :
        store_(store), repository_(repository)
    {}

    bool FavoriteService::SharingEnabled() const noexcept
    {
        std::scoped_lock lock(mutex_);
        return sharingEnabled_;
    }

    std::vector<SavedNpcEntry> FavoriteService::Merge(
        std::span<const SavedNpcEntry> shared,
        std::span<const SavedNpcEntry> current)
    {
        std::vector<SavedNpcEntry> combined;
        combined.reserve(shared.size() + current.size());
        combined.insert(combined.end(), shared.begin(), shared.end());
        combined.insert(combined.end(), current.begin(), current.end());
        SavedNpcStore normalized;
        normalized.ReplaceFavorites(combined);
        return normalized.Favorites();
    }

    std::expected<void, std::string> FavoriteService::CommitShared(
        std::span<const SavedNpcEntry> entries)
    {
        if (const auto saved = repository_.Save(entries); !saved) {
            return std::unexpected(saved.error());
        }
        store_.ReplaceFavorites(entries);
        return {};
    }

    std::expected<void, std::string> FavoriteService::SetSharingEnabled(bool enabled)
    {
        std::scoped_lock lock(mutex_);
        if (!enabled) {
            sharingEnabled_ = false;
            return {};
        }
        const auto shared = repository_.Load();
        if (!shared) return std::unexpected(shared.error());
        const auto current = store_.Favorites();
        const auto merged = Merge(*shared, current);
        if (const auto committed = CommitShared(merged); !committed) return committed;
        sharingEnabled_ = true;
        return {};
    }

    std::expected<void, std::string> FavoriteService::MergeSharedForSession()
    {
        std::scoped_lock lock(mutex_);
        if (!sharingEnabled_) return {};
        const auto shared = repository_.Load();
        if (!shared) return std::unexpected(shared.error());
        const auto current = store_.Favorites();
        return CommitShared(Merge(*shared, current));
    }

    std::expected<bool, std::string> FavoriteService::Toggle(SavedNpcEntry entry)
    {
        std::scoped_lock lock(mutex_);
        if (!entry.identity.IsPersistable()) {
            return std::unexpected("Dynamic NPCs cannot be stored as Favorites");
        }
        auto proposed = store_.Favorites();
        const auto existing = std::ranges::find(
            proposed, entry.identity, &SavedNpcEntry::identity);
        const bool nowFavorite = existing == proposed.end();
        if (nowFavorite) proposed.push_back(std::move(entry));
        else proposed.erase(existing);

        SavedNpcStore normalized;
        normalized.ReplaceFavorites(proposed);
        proposed = normalized.Favorites();
        if (sharingEnabled_) {
            if (const auto committed = CommitShared(proposed); !committed) {
                return std::unexpected(committed.error());
            }
        } else {
            store_.ReplaceFavorites(proposed);
        }
        return nowFavorite;
    }

    std::expected<void, std::string> FavoriteService::Remove(const FormIdentity& identity)
    {
        const std::array identities{identity};
        const auto removed = RemoveMany(identities);
        if (!removed) return std::unexpected(removed.error());
        return {};
    }

    std::expected<std::size_t, std::string> FavoriteService::RemoveMany(
        std::span<const FormIdentity> identities)
    {
        std::scoped_lock lock(mutex_);
        auto proposed = store_.Favorites();
        const auto oldSize = proposed.size();
        std::erase_if(proposed, [&](const SavedNpcEntry& entry) {
            return std::ranges::any_of(identities, [&](const FormIdentity& identity) {
                return entry.identity == identity;
            });
        });
        const auto removed = oldSize - proposed.size();
        if (removed == 0) return std::size_t{0};
        if (sharingEnabled_) {
            if (const auto committed = CommitShared(proposed); !committed) {
                return std::unexpected(committed.error());
            }
        } else {
            store_.ReplaceFavorites(proposed);
        }
        return removed;
    }

    std::expected<void, std::string> FavoriteService::Clear()
    {
        std::scoped_lock lock(mutex_);
        sharingEnabled_ = false;
        store_.ClearFavorites();
        return {};
    }

    std::expected<void, std::string> FavoriteService::ClearForUninstall()
    {
        std::scoped_lock lock(mutex_);
        if (const auto cleared = repository_.Clear(); !cleared) return cleared;
        sharingEnabled_ = false;
        store_.ClearFavorites();
        return {};
    }
}
