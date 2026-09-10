#include "Persistence/Serialization.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace whereabouts
{
    bool HasSerializableState(const SavedNpcStoreSnapshot& snapshot) noexcept
    {
        return !snapshot.favorites.empty() || !snapshot.recent.empty() ||
               !snapshot.trackedDeaths.empty() || snapshot.trackingWarningAcknowledged;
    }

    namespace
    {
        class Writer
        {
        public:
            template <class Integer>
            void IntegerValue(Integer value)
            {
                using Unsigned = std::make_unsigned_t<Integer>;
                auto remaining = static_cast<Unsigned>(value);
                for (std::size_t index = 0; index < sizeof(Integer); ++index) {
                    bytes_.push_back(static_cast<std::byte>(remaining & 0xFFU));
                    remaining >>= 8U;
                }
            }

            void StringValue(std::string_view value)
            {
                IntegerValue<std::uint16_t>(static_cast<std::uint16_t>(value.size()));
                for (const auto character : value) {
                    bytes_.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
                }
            }

            std::vector<std::byte> Take()
            {
                return std::move(bytes_);
            }

        private:
            std::vector<std::byte> bytes_;
        };

        class Reader
        {
        public:
            explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

            template <class Integer>
            std::expected<Integer, SerializationError> IntegerValue()
            {
                if (bytes_.size() - position_ < sizeof(Integer)) {
                    return std::unexpected(SerializationError{"The record ended unexpectedly"});
                }
                using Unsigned = std::make_unsigned_t<Integer>;
                Unsigned value = 0;
                for (std::size_t index = 0; index < sizeof(Integer); ++index) {
                    value |= static_cast<Unsigned>(std::to_integer<unsigned char>(bytes_[position_++])) << (index * 8U);
                }
                return static_cast<Integer>(value);
            }

            std::expected<std::string, SerializationError> StringValue(std::size_t maximum)
            {
                const auto length = IntegerValue<std::uint16_t>();
                if (!length) return std::unexpected(length.error());
                if (*length > maximum || bytes_.size() - position_ < *length) {
                    return std::unexpected(SerializationError{"A string length is invalid"});
                }
                std::string result;
                result.reserve(*length);
                for (std::size_t index = 0; index < *length; ++index) {
                    result.push_back(static_cast<char>(std::to_integer<unsigned char>(bytes_[position_++])));
                }
                return result;
            }

            [[nodiscard]] bool Finished() const noexcept
            {
                return position_ == bytes_.size();
            }

        private:
            std::span<const std::byte> bytes_;
            std::size_t position_{0};
        };

        bool IsValidUtf8(std::string_view value)
        {
            std::size_t index = 0;
            while (index < value.size()) {
                const auto lead = static_cast<unsigned char>(value[index]);
                std::size_t continuationCount = 0;
                std::uint32_t codePoint = 0;
                if (lead <= 0x7F) {
                    ++index;
                    continue;
                }
                if ((lead & 0xE0U) == 0xC0U) {
                    continuationCount = 1;
                    codePoint = lead & 0x1FU;
                    if (codePoint == 0) return false;
                } else if ((lead & 0xF0U) == 0xE0U) {
                    continuationCount = 2;
                    codePoint = lead & 0x0FU;
                } else if ((lead & 0xF8U) == 0xF0U) {
                    continuationCount = 3;
                    codePoint = lead & 0x07U;
                } else {
                    return false;
                }
                if (index + continuationCount >= value.size()) return false;
                for (std::size_t offset = 1; offset <= continuationCount; ++offset) {
                    const auto continuation = static_cast<unsigned char>(value[index + offset]);
                    if ((continuation & 0xC0U) != 0x80U) return false;
                    codePoint = (codePoint << 6U) | (continuation & 0x3FU);
                }
                if ((continuationCount == 1 && codePoint < 0x80U) ||
                    (continuationCount == 2 && codePoint < 0x800U) ||
                    (continuationCount == 3 && codePoint < 0x10000U) ||
                    codePoint > 0x10FFFFU ||
                    (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
                    return false;
                }
                index += continuationCount + 1;
            }
            return true;
        }

        std::optional<SerializationError> ValidateEntry(const SavedNpcEntry& entry)
        {
            if (!entry.identity.IsPersistable()) {
                return SerializationError{"The entry has no stable form identity"};
            }
            if (entry.identity.plugin.size() > kMaximumPluginNameBytes ||
                entry.lastKnownName.size() > kMaximumLastKnownNameBytes) {
                return SerializationError{"An entry string exceeds the supported length"};
            }
            if (!IsValidUtf8(entry.identity.plugin) || !IsValidUtf8(entry.lastKnownName)) {
                return SerializationError{"An entry contains invalid UTF-8"};
            }
            const auto maximumLocalID = entry.identity.light ? 0xFFFU : 0xFFFFFFU;
            if (entry.identity.localID > maximumLocalID) {
                return SerializationError{"The local FormID does not match the plugin kind"};
            }
            return std::nullopt;
        }

        std::optional<SerializationError> ValidateTrackedDeathEntry(
            const TrackedDeathEntry& entry)
        {
            if (entry.runtimeFormID == 0) {
                return SerializationError{"The tracked-death entry has no runtime FormID"};
            }
            if (entry.identity) {
                const SavedNpcEntry identityEntry{*entry.identity, entry.lastKnownName};
                if (const auto error = ValidateEntry(identityEntry)) return error;
            } else if (entry.lastKnownName.size() > kMaximumLastKnownNameBytes ||
                       !IsValidUtf8(entry.lastKnownName)) {
                return SerializationError{"The tracked-death name is invalid"};
            }
            if (entry.lastKnownLocation.size() > kMaximumLastKnownLocationBytes ||
                !IsValidUtf8(entry.lastKnownLocation)) {
                return SerializationError{"The tracked-death location is invalid"};
            }
            if (entry.state != TrackedDeathState::BodyPresent &&
                entry.state != TrackedDeathState::BodyMissing) {
                return SerializationError{"The tracked-death state is invalid"};
            }
            if (entry.state == TrackedDeathState::BodyPresent &&
                entry.objectiveIndex >= kMaximumTrackedObjectives) {
                return SerializationError{"The tracked objective index is invalid"};
            }
            if (entry.state == TrackedDeathState::BodyMissing &&
                entry.objectiveIndex != kNoTrackedObjective) {
                return SerializationError{"A missing body cannot retain a tracked objective"};
            }
            return std::nullopt;
        }

        bool SameTrackedDeathIdentity(
            const TrackedDeathEntry& left,
            const TrackedDeathEntry& right) noexcept
        {
            if (left.identity || right.identity) {
                return left.identity && right.identity && *left.identity == *right.identity;
            }
            return left.runtimeFormID == right.runtimeFormID;
        }
    }

    EncodedSavedNpcEntries EncodeSavedNpcEntries(std::span<const SavedNpcEntry> entries)
    {
        if (entries.size() > kMaximumSavedNpcEntries) {
            return std::unexpected(SerializationError{"The entry count exceeds the supported limit"});
        }

        Writer writer;
        writer.IntegerValue(kSavedNpcSchemaVersion);
        writer.IntegerValue(static_cast<std::uint32_t>(entries.size()));
        for (const auto& entry : entries) {
            if (const auto error = ValidateEntry(entry)) {
                return std::unexpected(*error);
            }
            writer.StringValue(entry.identity.plugin);
            writer.IntegerValue(entry.identity.localID);
            writer.IntegerValue<std::uint8_t>(entry.identity.light ? 1U : 0U);
            writer.StringValue(entry.lastKnownName);
        }
        return writer.Take();
    }

    DecodedSavedNpcEntries DecodeSavedNpcEntries(std::span<const std::byte> bytes)
    {
        Reader reader(bytes);
        const auto version = reader.IntegerValue<std::uint32_t>();
        if (!version) return std::unexpected(version.error());
        if (*version != kSavedNpcSchemaVersion) {
            return std::unexpected(SerializationError{"The record schema version is unsupported"});
        }
        const auto count = reader.IntegerValue<std::uint32_t>();
        if (!count) return std::unexpected(count.error());
        if (*count > kMaximumSavedNpcEntries) {
            return std::unexpected(SerializationError{"The entry count exceeds the supported limit"});
        }

        std::vector<SavedNpcEntry> entries;
        entries.reserve(*count);
        for (std::uint32_t index = 0; index < *count; ++index) {
            auto plugin = reader.StringValue(kMaximumPluginNameBytes);
            if (!plugin) return std::unexpected(plugin.error());
            const auto localID = reader.IntegerValue<std::uint32_t>();
            if (!localID) return std::unexpected(localID.error());
            const auto light = reader.IntegerValue<std::uint8_t>();
            if (!light) return std::unexpected(light.error());
            if (*light > 1) {
                return std::unexpected(SerializationError{"The light-plugin flag is invalid"});
            }
            auto name = reader.StringValue(kMaximumLastKnownNameBytes);
            if (!name) return std::unexpected(name.error());

            SavedNpcEntry entry{{std::move(*plugin), *localID, *light != 0}, std::move(*name)};
            if (const auto error = ValidateEntry(entry)) {
                return std::unexpected(*error);
            }
            entries.push_back(std::move(entry));
        }
        if (!reader.Finished()) {
            return std::unexpected(SerializationError{"The record contains trailing data"});
        }
        return entries;
    }

    EncodedTrackedDeathEntries EncodeTrackedDeathEntries(
        std::span<const TrackedDeathEntry> entries)
    {
        if (entries.size() > kMaximumTrackedDeathEntries) {
            return std::unexpected(SerializationError{"The tracked-death count exceeds the supported limit"});
        }

        Writer writer;
        writer.IntegerValue(kTrackedDeathSchemaVersion);
        writer.IntegerValue(static_cast<std::uint32_t>(entries.size()));
        for (const auto& entry : entries) {
            if (const auto error = ValidateTrackedDeathEntry(entry)) {
                return std::unexpected(*error);
            }
            writer.IntegerValue(entry.runtimeFormID);
            writer.IntegerValue<std::uint8_t>(entry.identity ? 1U : 0U);
            if (entry.identity) {
                writer.StringValue(entry.identity->plugin);
                writer.IntegerValue(entry.identity->localID);
                writer.IntegerValue<std::uint8_t>(entry.identity->light ? 1U : 0U);
            }
            writer.IntegerValue(entry.objectiveIndex);
            writer.IntegerValue<std::uint8_t>(static_cast<std::uint8_t>(entry.state));
            writer.StringValue(entry.lastKnownName);
            writer.StringValue(entry.lastKnownLocation);
        }
        return writer.Take();
    }

    DecodedTrackedDeathEntries DecodeTrackedDeathEntries(
        std::span<const std::byte> bytes)
    {
        Reader reader(bytes);
        const auto version = reader.IntegerValue<std::uint32_t>();
        if (!version) return std::unexpected(version.error());
        if (*version != kTrackedDeathSchemaVersion) {
            return std::unexpected(SerializationError{"The tracked-death schema version is unsupported"});
        }
        const auto count = reader.IntegerValue<std::uint32_t>();
        if (!count) return std::unexpected(count.error());
        if (*count > kMaximumTrackedDeathEntries) {
            return std::unexpected(SerializationError{"The tracked-death count exceeds the supported limit"});
        }

        std::vector<TrackedDeathEntry> entries;
        entries.reserve(*count);
        for (std::uint32_t index = 0; index < *count; ++index) {
            const auto runtimeFormID = reader.IntegerValue<std::uint32_t>();
            if (!runtimeFormID) return std::unexpected(runtimeFormID.error());
            const auto hasIdentity = reader.IntegerValue<std::uint8_t>();
            if (!hasIdentity) return std::unexpected(hasIdentity.error());
            if (*hasIdentity > 1) {
                return std::unexpected(SerializationError{"The stable-identity flag is invalid"});
            }

            std::optional<FormIdentity> identity;
            if (*hasIdentity != 0) {
                auto plugin = reader.StringValue(kMaximumPluginNameBytes);
                if (!plugin) return std::unexpected(plugin.error());
                const auto localID = reader.IntegerValue<std::uint32_t>();
                if (!localID) return std::unexpected(localID.error());
                const auto light = reader.IntegerValue<std::uint8_t>();
                if (!light) return std::unexpected(light.error());
                if (*light > 1) {
                    return std::unexpected(SerializationError{"The light-plugin flag is invalid"});
                }
                identity = FormIdentity{std::move(*plugin), *localID, *light != 0};
            }

            const auto objectiveIndex = reader.IntegerValue<std::uint16_t>();
            if (!objectiveIndex) return std::unexpected(objectiveIndex.error());
            const auto rawState = reader.IntegerValue<std::uint8_t>();
            if (!rawState) return std::unexpected(rawState.error());
            if (*rawState > static_cast<std::uint8_t>(TrackedDeathState::BodyMissing)) {
                return std::unexpected(SerializationError{"The tracked-death state is invalid"});
            }
            auto name = reader.StringValue(kMaximumLastKnownNameBytes);
            if (!name) return std::unexpected(name.error());
            auto location = reader.StringValue(kMaximumLastKnownLocationBytes);
            if (!location) return std::unexpected(location.error());

            TrackedDeathEntry entry{
                *runtimeFormID,
                std::move(identity),
                *objectiveIndex,
                static_cast<TrackedDeathState>(*rawState),
                std::move(*name),
                std::move(*location)};
            if (const auto error = ValidateTrackedDeathEntry(entry)) {
                return std::unexpected(*error);
            }
            entries.push_back(std::move(entry));
        }
        if (!reader.Finished()) {
            return std::unexpected(SerializationError{"The tracked-death record contains trailing data"});
        }
        return entries;
    }

    std::array<std::byte, 1> EncodeTrackingWarningAcknowledgement(bool acknowledged) noexcept
    {
        return {acknowledged ? std::byte{1} : std::byte{0}};
    }

    std::expected<bool, SerializationError> DecodeTrackingWarningAcknowledgement(
        std::span<const std::byte> bytes)
    {
        if (bytes.size() != 1 || (bytes.front() != std::byte{0} && bytes.front() != std::byte{1})) {
            return std::unexpected(SerializationError{"The tracking-warning record is invalid"});
        }
        return bytes.front() == std::byte{1};
    }

    bool SavedNpcStore::AddFavorite(SavedNpcEntry entry)
    {
        if (ValidateEntry(entry)) return false;
        std::scoped_lock lock(mutex_);
        const auto existing = std::find_if(favorites_.begin(), favorites_.end(), [&](const auto& value) {
            return value.identity == entry.identity;
        });
        if (existing != favorites_.end()) {
            existing->lastKnownName = std::move(entry.lastKnownName);
            return false;
        }
        if (favorites_.size() >= kMaximumSavedNpcEntries) return false;
        favorites_.push_back(std::move(entry));
        return true;
    }

    bool SavedNpcStore::RemoveFavorite(const FormIdentity& identity)
    {
        std::scoped_lock lock(mutex_);
        const auto existing = std::find_if(favorites_.begin(), favorites_.end(), [&](const auto& value) {
            return value.identity == identity;
        });
        if (existing == favorites_.end()) return false;
        favorites_.erase(existing);
        return true;
    }

    std::size_t SavedNpcStore::RemoveFavorites(std::span<const FormIdentity> identities)
    {
        std::scoped_lock lock(mutex_);
        return std::erase_if(favorites_, [&](const SavedNpcEntry& entry) {
            return std::ranges::any_of(identities, [&](const FormIdentity& identity) {
                return entry.identity == identity;
            });
        });
    }

    bool SavedNpcStore::RecordRecent(SavedNpcEntry entry)
    {
        if (ValidateEntry(entry)) return false;
        std::scoped_lock lock(mutex_);
        const auto existing = std::find_if(recent_.begin(), recent_.end(), [&](const auto& value) {
            return value.identity == entry.identity;
        });
        const bool inserted = existing == recent_.end();
        const bool moved = !inserted && existing != recent_.begin();
        if (!inserted) {
            recent_.erase(existing);
        }
        recent_.insert(recent_.begin(), std::move(entry));
        TrimRecent();
        return inserted || moved;
    }

    bool SavedNpcStore::RemoveRecent(const FormIdentity& identity)
    {
        std::scoped_lock lock(mutex_);
        const auto existing = std::find_if(recent_.begin(), recent_.end(), [&](const auto& value) {
            return value.identity == identity;
        });
        if (existing == recent_.end()) return false;
        recent_.erase(existing);
        return true;
    }

    std::size_t SavedNpcStore::RemoveRecent(std::span<const FormIdentity> identities)
    {
        std::scoped_lock lock(mutex_);
        return std::erase_if(recent_, [&](const SavedNpcEntry& entry) {
            return std::ranges::any_of(identities, [&](const FormIdentity& identity) {
                return entry.identity == identity;
            });
        });
    }

    void SavedNpcStore::SetRecentLimit(std::size_t limit)
    {
        std::scoped_lock lock(mutex_);
        recentLimit_ = std::clamp<std::size_t>(limit, 1, 100);
        TrimRecent();
    }

    void SavedNpcStore::ReplaceFavorites(std::span<const SavedNpcEntry> entries)
    {
        std::scoped_lock lock(mutex_);
        favorites_.clear();
        for (const auto& entry : entries) {
            if (favorites_.size() >= kMaximumSavedNpcEntries) break;
            if (ValidateEntry(entry)) continue;
            const auto existing = std::find_if(favorites_.begin(), favorites_.end(), [&](const auto& value) {
                return value.identity == entry.identity;
            });
            if (existing == favorites_.end()) favorites_.push_back(entry);
        }
    }

    void SavedNpcStore::ReplaceRecent(std::span<const SavedNpcEntry> entries)
    {
        std::scoped_lock lock(mutex_);
        recent_.clear();
        for (const auto& entry : entries) {
            if (ValidateEntry(entry)) continue;
            const auto existing = std::find_if(recent_.begin(), recent_.end(), [&](const auto& value) {
                return value.identity == entry.identity;
            });
            if (existing == recent_.end()) recent_.push_back(entry);
        }
        TrimRecent();
    }

    bool SavedNpcStore::UpsertTrackedDeath(TrackedDeathEntry entry)
    {
        if (ValidateTrackedDeathEntry(entry)) return false;
        std::scoped_lock lock(mutex_);
        const auto existing = std::find_if(
            trackedDeaths_.begin(),
            trackedDeaths_.end(),
            [&](const auto& value) { return SameTrackedDeathIdentity(value, entry); });
        const bool inserted = existing == trackedDeaths_.end();
        if (!inserted) trackedDeaths_.erase(existing);
        if (inserted && trackedDeaths_.size() >= kMaximumTrackedDeathEntries) return false;
        trackedDeaths_.insert(trackedDeaths_.begin(), std::move(entry));
        return inserted;
    }

    bool SavedNpcStore::RemoveTrackedDeath(std::uint32_t runtimeFormID)
    {
        if (runtimeFormID == 0) return false;
        std::scoped_lock lock(mutex_);
        if (std::ranges::count_if(trackedDeaths_, [&](const auto& entry) {
                return entry.runtimeFormID == runtimeFormID;
            }) != 1) return false;
        return std::erase_if(trackedDeaths_, [&](const TrackedDeathEntry& entry) {
            return entry.runtimeFormID == runtimeFormID;
        }) != 0;
    }

    bool SavedNpcStore::RemoveTrackedDeath(const TrackedDeathEntry& identity)
    {
        std::scoped_lock lock(mutex_);
        return std::erase_if(trackedDeaths_, [&](const auto& entry) {
            return SameTrackedDeathIdentity(entry, identity);
        }) != 0;
    }

    void SavedNpcStore::ReplaceTrackedDeaths(std::span<const TrackedDeathEntry> entries)
    {
        std::scoped_lock lock(mutex_);
        trackedDeaths_.clear();
        for (const auto& entry : entries) {
            if (trackedDeaths_.size() >= kMaximumTrackedDeathEntries) break;
            if (ValidateTrackedDeathEntry(entry)) continue;
            const auto duplicate = std::ranges::any_of(trackedDeaths_, [&](const auto& value) {
                return SameTrackedDeathIdentity(value, entry);
            });
            if (!duplicate) trackedDeaths_.push_back(entry);
        }
    }

    void SavedNpcStore::Clear()
    {
        std::scoped_lock lock(mutex_);
        favorites_.clear();
        recent_.clear();
        trackedDeaths_.clear();
        trackingWarningAcknowledged_ = false;
    }

    void SavedNpcStore::ReplaceAll(const SavedNpcStoreSnapshot& snapshot)
    {
        std::vector<SavedNpcEntry> favorites;
        favorites.reserve(snapshot.favorites.size());
        for (const auto& entry : snapshot.favorites) {
            if (favorites.size() >= kMaximumSavedNpcEntries || ValidateEntry(entry)) continue;
            if (!std::ranges::any_of(favorites, [&](const auto& value) {
                    return value.identity == entry.identity;
                })) {
                favorites.push_back(entry);
            }
        }

        std::vector<SavedNpcEntry> recent;
        recent.reserve(snapshot.recent.size());
        for (const auto& entry : snapshot.recent) {
            if (recent.size() >= kMaximumSavedNpcEntries || ValidateEntry(entry)) continue;
            if (!std::ranges::any_of(recent, [&](const auto& value) {
                    return value.identity == entry.identity;
                })) {
                recent.push_back(entry);
            }
        }

        std::vector<TrackedDeathEntry> deaths;
        deaths.reserve(snapshot.trackedDeaths.size());
        for (const auto& entry : snapshot.trackedDeaths) {
            if (deaths.size() >= kMaximumTrackedDeathEntries || ValidateTrackedDeathEntry(entry)) continue;
            if (!std::ranges::any_of(deaths, [&](const auto& value) {
                    return SameTrackedDeathIdentity(value, entry);
                })) {
                deaths.push_back(entry);
            }
        }

        std::scoped_lock lock(mutex_);
        favorites_ = std::move(favorites);
        recent_ = std::move(recent);
        trackedDeaths_ = std::move(deaths);
        trackingWarningAcknowledged_ = snapshot.trackingWarningAcknowledged;
        TrimRecent();
    }

    void SavedNpcStore::ClearFavorites()
    {
        std::scoped_lock lock(mutex_);
        favorites_.clear();
    }

    void SavedNpcStore::ClearRecent()
    {
        std::scoped_lock lock(mutex_);
        recent_.clear();
    }

    void SavedNpcStore::ClearTrackedDeaths()
    {
        std::scoped_lock lock(mutex_);
        trackedDeaths_.clear();
    }

    SavedNpcStoreSnapshot SavedNpcStore::SnapshotAll() const
    {
        std::scoped_lock lock(mutex_);
        return {favorites_, recent_, trackedDeaths_, trackingWarningAcknowledged_};
    }

    std::vector<SavedNpcEntry> SavedNpcStore::Favorites() const
    {
        std::scoped_lock lock(mutex_);
        return favorites_;
    }

    std::vector<SavedNpcEntry> SavedNpcStore::Recent() const
    {
        std::scoped_lock lock(mutex_);
        return recent_;
    }

    std::vector<TrackedDeathEntry> SavedNpcStore::TrackedDeaths() const
    {
        std::scoped_lock lock(mutex_);
        return trackedDeaths_;
    }

    bool SavedNpcStore::TrackingWarningAcknowledged() const
    {
        std::scoped_lock lock(mutex_);
        return trackingWarningAcknowledged_;
    }

    void SavedNpcStore::AcknowledgeTrackingWarning()
    {
        std::scoped_lock lock(mutex_);
        trackingWarningAcknowledged_ = true;
    }

    void SavedNpcStore::TrimRecent()
    {
        if (recent_.size() > recentLimit_) recent_.resize(recentLimit_);
    }
}
