#include "Search/PluginStructure.h"

#include <array>
#include <cstring>
#include <optional>

namespace whereabouts
{
    namespace
    {
        constexpr std::uint64_t kHeaderSize = 24;
        constexpr std::uint32_t kGroupSignature = 0x50555247;  // GRUP
        constexpr std::uint32_t kActorReferenceSignature = 0x52484341;  // ACHR

        struct GroupFrame
        {
            std::uint64_t end{0};
            std::optional<std::uint32_t> cell;
        };

        [[nodiscard]] std::uint32_t ReadU32(
            const std::array<std::byte, kHeaderSize>& header,
            std::size_t offset) noexcept
        {
            std::uint32_t value = 0;
            std::memcpy(&value, header.data() + offset, sizeof(value));
            return value;
        }

        [[nodiscard]] constexpr bool IsCellChildrenGroup(std::uint32_t type) noexcept
        {
            return type == 6 || type == 8 || type == 9;
        }
    }

    std::expected<std::vector<PlacedRecordCellParent>, PluginStructureError>
        ScanPlacedRecordCellParents(
            std::uint64_t fileSize,
            const PluginReadAt& readAt)
    {
        if (!readAt || fileSize < kHeaderSize) {
            return std::unexpected(PluginStructureError::InvalidHeader);
        }

        std::vector<PlacedRecordCellParent> parents;
        std::vector<GroupFrame> groups;
        groups.push_back({fileSize, std::nullopt});
        std::uint64_t offset = 0;

        while (offset < fileSize) {
            while (groups.size() > 1 && offset == groups.back().end) groups.pop_back();
            if (offset > groups.back().end || groups.back().end - offset < kHeaderSize) {
                return std::unexpected(PluginStructureError::InvalidBounds);
            }

            std::array<std::byte, kHeaderSize> header{};
            if (!readAt(offset, header)) {
                return std::unexpected(PluginStructureError::ReadFailed);
            }

            const auto signature = ReadU32(header, 0);
            if (signature == kGroupSignature) {
                const auto groupSize = static_cast<std::uint64_t>(ReadU32(header, 4));
                if (groupSize < kHeaderSize || groupSize > groups.back().end - offset) {
                    return std::unexpected(PluginStructureError::InvalidBounds);
                }
                auto cell = groups.back().cell;
                if (IsCellChildrenGroup(ReadU32(header, 12))) cell = ReadU32(header, 8);
                groups.push_back({offset + groupSize, cell});
                offset += kHeaderSize;
                continue;
            }

            const auto dataSize = static_cast<std::uint64_t>(ReadU32(header, 4));
            const auto recordSize = kHeaderSize + dataSize;
            if (recordSize > groups.back().end - offset) {
                return std::unexpected(PluginStructureError::InvalidBounds);
            }
            if (signature == kActorReferenceSignature && groups.back().cell) {
                parents.push_back({ReadU32(header, 12), *groups.back().cell});
            }
            offset += recordSize;
        }

        while (groups.size() > 1 && offset == groups.back().end) groups.pop_back();
        if (groups.size() != 1 || offset != fileSize) {
            return std::unexpected(PluginStructureError::InvalidBounds);
        }
        return parents;
    }
}
