#include "PCH.h"

#include "AppContext.h"
#include "Lifecycle/ProcessContext.h"
#include "Persistence/Serialization.h"
#include "Persistence/SkseSerialization.h"
#include "Lifecycle/CallbackGuard.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace whereabouts
{
    namespace
    {
        constexpr std::uint32_t FourCharacterCode(char first, char second, char third, char fourth) noexcept
        {
            return static_cast<std::uint32_t>(static_cast<unsigned char>(first)) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(second)) << 8U) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(third)) << 16U) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(fourth)) << 24U);
        }

        constexpr auto kSerializationID = FourCharacterCode('W', 'H', 'R', 'B');
        constexpr auto kFavoritesRecord = FourCharacterCode('F', 'A', 'V', 'R');
        constexpr auto kRecentRecord = FourCharacterCode('R', 'C', 'N', 'T');
        constexpr auto kTrackingWarningRecord = FourCharacterCode('T', 'W', 'A', 'R');
        constexpr auto kTrackedDeathRecord = FourCharacterCode('D', 'T', 'H', 'S');
        constexpr std::uint32_t kRecordVersion = 1;
        constexpr std::uint32_t kMaximumRecordBytes = 6U * 1024U * 1024U;

        class SerializationBoundaryScope
        {
        public:
            SerializationBoundaryScope() noexcept :
                context_(AcquireProcessContext()),
                token_(context_ ? context_->BeginSessionBoundary() : OperationEpochToken{})
            {}
            ~SerializationBoundaryScope()
            {
                if (context_ && token_) static_cast<void>(context_->ResumeSession(token_));
            }

        private:
            AppContext* context_{nullptr};
            OperationEpochToken token_;
        };

        bool WriteEntries(
            SKSE::SerializationInterface* serialization,
            std::uint32_t type,
            std::span<const SavedNpcEntry> entries)
        {
            const auto encoded = EncodeSavedNpcEntries(entries);
            if (!encoded) {
                logger::error("Could not encode save record: {}", encoded.error().message);
                return false;
            }
            return serialization->WriteRecord(
                type,
                kRecordVersion,
                encoded->data(),
                static_cast<std::uint32_t>(encoded->size()));
        }

        bool WriteTrackedDeaths(
            SKSE::SerializationInterface* serialization,
            std::span<const TrackedDeathEntry> entries)
        {
            const auto encoded = EncodeTrackedDeathEntries(entries);
            if (!encoded) {
                logger::error("Could not encode tracked-death save record: {}", encoded.error().message);
                return false;
            }
            return serialization->WriteRecord(
                kTrackedDeathRecord,
                kRecordVersion,
                encoded->data(),
                static_cast<std::uint32_t>(encoded->size()));
        }

        void SaveImpl(SKSE::SerializationInterface* serialization)
        {
            auto* context = AcquireProcessContext();
            if (!serialization || !context) return;
            const auto snapshot = context->savedNpcs.SnapshotAll();
            if (!HasSerializableState(snapshot)) {
                logger::info("Whereabouts save state is empty; no SKSE serialization records written");
                return;
            }
            const bool favoritesWritten = WriteEntries(
                serialization, kFavoritesRecord, snapshot.favorites);
            const bool recentWritten = WriteEntries(
                serialization, kRecentRecord, snapshot.recent);
            const bool trackedDeathsWritten = WriteTrackedDeaths(
                serialization, snapshot.trackedDeaths);
            const auto warning = EncodeTrackingWarningAcknowledgement(
                snapshot.trackingWarningAcknowledged);
            const bool warningWritten = serialization->WriteRecord(
                kTrackingWarningRecord,
                kRecordVersion,
                warning.data(),
                static_cast<std::uint32_t>(warning.size()));
            if (!favoritesWritten) {
                logger::error("Could not write the favorites save record");
            }
            if (!recentWritten) {
                logger::error("Could not write the recent-history save record");
            }
            if (!trackedDeathsWritten) {
                logger::error("Could not write the tracked-death save record");
            }
            if (!warningWritten) {
                logger::error("Could not write the tracking-warning acknowledgement record");
            }
            if (favoritesWritten && recentWritten && trackedDeathsWritten && warningWritten) {
                logger::info(
                    "Saved Whereabouts schema {}: {} favorites, {} recent entries, {} tracked deaths, warning acknowledged {}",
                    kRecordVersion,
                    snapshot.favorites.size(),
                    snapshot.recent.size(),
                    snapshot.trackedDeaths.size(),
                    snapshot.trackingWarningAcknowledged);
            }
        }

        void DiscardRecord(SKSE::SerializationInterface* serialization, std::uint32_t length)
        {
            std::array<std::byte, 4096> buffer{};
            while (length > 0) {
                const auto chunk = std::min<std::uint32_t>(length, static_cast<std::uint32_t>(buffer.size()));
                const auto read = serialization->ReadRecordData(buffer.data(), chunk);
                if (read == 0) break;
                length -= read;
            }
        }

        void LoadImpl(SKSE::SerializationInterface* serialization)
        {
            auto* context = AcquireProcessContext();
            if (!serialization || !context) return;
            SavedNpcStoreSnapshot loaded;

            std::uint32_t type = 0;
            std::uint32_t version = 0;
            std::uint32_t length = 0;
            while (serialization->GetNextRecordInfo(type, version, length)) {
                if (type != kFavoritesRecord && type != kRecentRecord &&
                    type != kTrackingWarningRecord && type != kTrackedDeathRecord) {
                    DiscardRecord(serialization, length);
                    continue;
                }
                if (version != kRecordVersion || length > kMaximumRecordBytes) {
                    logger::warn("Ignoring unsupported or oversized Whereabouts save record");
                    DiscardRecord(serialization, length);
                    continue;
                }

                if (type == kTrackingWarningRecord) {
                    std::vector<std::byte> bytes(length);
                    if (serialization->ReadRecordData(bytes.data(), length) != length) {
                        logger::warn("Ignoring truncated tracking-warning acknowledgement record");
                        continue;
                    }
                    const auto acknowledged = DecodeTrackingWarningAcknowledgement(bytes);
                    if (!acknowledged) {
                        logger::warn("Ignoring invalid tracking-warning acknowledgement record: {}",
                            acknowledged.error().message);
                    } else loaded.trackingWarningAcknowledged = *acknowledged;
                    continue;
                }

                if (type == kTrackedDeathRecord) {
                    std::vector<std::byte> bytes(length);
                    if (serialization->ReadRecordData(bytes.data(), length) != length) {
                        logger::warn("Ignoring truncated tracked-death save record");
                        continue;
                    }
                    auto decoded = DecodeTrackedDeathEntries(bytes);
                    if (!decoded) {
                        logger::warn(
                            "Ignoring invalid tracked-death save record: {}",
                            decoded.error().message);
                        continue;
                    }
                    for (auto& entry : *decoded) {
                        RE::FormID resolvedFormID = 0;
                        if (serialization->ResolveFormID(entry.runtimeFormID, resolvedFormID)) {
                            entry.runtimeFormID = resolvedFormID;
                        }
                    }
                    loaded.trackedDeaths = std::move(*decoded);
                    continue;
                }

                std::vector<std::byte> bytes(length);
                if (serialization->ReadRecordData(bytes.data(), length) != length) {
                    logger::warn("Ignoring truncated Whereabouts save record");
                    continue;
                }
                const auto decoded = DecodeSavedNpcEntries(bytes);
                if (!decoded) {
                    logger::warn("Ignoring invalid Whereabouts save record: {}", decoded.error().message);
                    continue;
                }
                if (type == kFavoritesRecord) {
                    loaded.favorites = std::move(*decoded);
                } else {
                    loaded.recent = std::move(*decoded);
                }
            }
            context->savedNpcs.ReplaceAll(loaded);
            const auto snapshot = context->savedNpcs.SnapshotAll();
            logger::info(
                "Loaded Whereabouts schema {}: {} favorites, {} recent entries, {} tracked deaths, warning acknowledged {}",
                kRecordVersion,
                snapshot.favorites.size(),
                snapshot.recent.size(),
                snapshot.trackedDeaths.size(),
                snapshot.trackingWarningAcknowledged);
        }

        void RevertImpl(SKSE::SerializationInterface*)
        {
            const SerializationBoundaryScope boundary;
            if (auto* context = AcquireProcessContext()) context->savedNpcs.Clear();
        }

        void LogSerializationCallbackException() noexcept
        {
            try { logger::error("Contained exception in an SKSE serialization callback"); }
            catch (...) {}
        }

        void HandleLoadCallbackException() noexcept
        {
            try {
                if (auto* context = AcquireProcessContext()) context->savedNpcs.Clear();
            } catch (...) {
            }
            LogSerializationCallbackException();
        }

        void Save(SKSE::SerializationInterface* serialization)
        {
            GuardCallbackVoid(
                [serialization] { SaveImpl(serialization); },
                LogSerializationCallbackException);
        }

        void Load(SKSE::SerializationInterface* serialization)
        {
            const SerializationBoundaryScope boundary;
            GuardCallbackVoid(
                [serialization] { LoadImpl(serialization); },
                HandleLoadCallbackException);
        }

        void Revert(SKSE::SerializationInterface* serialization)
        {
            GuardCallbackVoid(
                [serialization] { RevertImpl(serialization); },
                LogSerializationCallbackException);
        }
    }

    bool RegisterSkseSerialization()
    {
        const auto* serialization = SKSE::GetSerializationInterface();
        if (!serialization) return false;

        serialization->SetUniqueID(kSerializationID);
        serialization->SetSaveCallback(Save);
        serialization->SetLoadCallback(Load);
        serialization->SetRevertCallback(Revert);
        return true;
    }
}
