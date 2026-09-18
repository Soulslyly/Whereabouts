#pragma once

#include "Persistence/Serialization.h"

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace whereabouts
{
    inline constexpr std::uint32_t kSharedFavoritesFormatVersion = 1;

    class SharedFavoritesRepository
    {
    public:
        explicit SharedFavoritesRepository(std::filesystem::path path);

        [[nodiscard]] std::expected<std::vector<SavedNpcEntry>, std::string> Load() const;
        [[nodiscard]] std::expected<void, std::string> Save(
            std::span<const SavedNpcEntry> entries) const;
        [[nodiscard]] std::expected<void, std::string> Clear() const;
        [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

    private:
        std::filesystem::path path_;
    };
}
