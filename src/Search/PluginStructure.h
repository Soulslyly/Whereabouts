#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <vector>

namespace whereabouts
{
    struct PlacedRecordCellParent
    {
        std::uint32_t rawReferenceFormID{0};
        std::uint32_t rawCellFormID{0};

        [[nodiscard]] bool operator==(const PlacedRecordCellParent&) const noexcept = default;
    };

    enum class PluginStructureError
    {
        ReadFailed,
        InvalidHeader,
        InvalidBounds
    };

    using PluginReadAt = std::function<bool(std::uint64_t, std::span<std::byte>)>;

    [[nodiscard]] std::expected<std::vector<PlacedRecordCellParent>, PluginStructureError>
        ScanPlacedRecordCellParents(std::uint64_t fileSize, const PluginReadAt& readAt);
}
