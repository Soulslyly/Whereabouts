#include "Persistence/SharedFavorites.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace whereabouts
{
    namespace
    {
        constexpr std::array<std::byte, 4> kMagic{
            std::byte{'W'}, std::byte{'A'}, std::byte{'F'}, std::byte{'V'}};
        constexpr std::uintmax_t kMaximumSharedFavoritesBytes = 8U * 1024U * 1024U;

        std::string WindowsError(std::string_view action, DWORD error)
        {
            return std::string(action) + " (Windows error " + std::to_string(error) + ')';
        }

        void AppendVersion(std::vector<std::byte>& bytes, std::uint32_t version)
        {
            for (unsigned shift = 0; shift < 32; shift += 8) {
                bytes.push_back(static_cast<std::byte>((version >> shift) & 0xFFU));
            }
        }

        std::uint32_t ReadVersion(std::span<const std::byte> bytes)
        {
            std::uint32_t result = 0;
            for (unsigned index = 0; index < 4; ++index) {
                result |= std::to_integer<std::uint32_t>(bytes[index]) << (index * 8);
            }
            return result;
        }
    }

    SharedFavoritesRepository::SharedFavoritesRepository(std::filesystem::path path) :
        path_(std::move(path))
    {}

    std::expected<std::vector<SavedNpcEntry>, std::string>
    SharedFavoritesRepository::Load() const
    {
        if (path_.empty()) return std::unexpected("The SKSE user-data directory is unavailable");
        std::error_code error;
        if (!std::filesystem::exists(path_, error)) {
            if (error) return std::unexpected("Could not inspect the shared Favorites file: " + error.message());
            return std::vector<SavedNpcEntry>{};
        }
        const auto size = std::filesystem::file_size(path_, error);
        if (error) return std::unexpected("Could not size the shared Favorites file: " + error.message());
        if (size < 8 || size > kMaximumSharedFavoritesBytes) {
            return std::unexpected("The shared Favorites file has an invalid size");
        }

        std::ifstream input(path_, std::ios::binary);
        if (!input) return std::unexpected("Could not open the shared Favorites file");
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input) return std::unexpected("Could not read the shared Favorites file");
        if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
            return std::unexpected("The shared Favorites file magic is invalid");
        }
        if (ReadVersion(std::span<const std::byte>{bytes}.subspan(4, 4)) !=
            kSharedFavoritesFormatVersion) {
            return std::unexpected("The shared Favorites file version is unsupported");
        }
        const auto decoded = DecodeSavedNpcEntries(
            std::span<const std::byte>{bytes}.subspan(8));
        if (!decoded) return std::unexpected(decoded.error().message);
        return *decoded;
    }

    std::expected<void, std::string> SharedFavoritesRepository::Save(
        std::span<const SavedNpcEntry> entries) const
    {
        if (path_.empty()) return std::unexpected("The SKSE user-data directory is unavailable");
        const auto encoded = EncodeSavedNpcEntries(entries);
        if (!encoded) return std::unexpected(encoded.error().message);

        std::vector<std::byte> bytes;
        bytes.reserve(8 + encoded->size());
        bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
        AppendVersion(bytes, kSharedFavoritesFormatVersion);
        bytes.insert(bytes.end(), encoded->begin(), encoded->end());

        std::error_code directoryError;
        if (!path_.parent_path().empty()) {
            std::filesystem::create_directories(path_.parent_path(), directoryError);
            if (directoryError) {
                return std::unexpected(
                    "Could not create the shared Favorites directory: " + directoryError.message());
            }
        }
        const auto temporaryPath = std::filesystem::path(path_.wstring() + L".tmp");
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) return std::unexpected("Could not open the temporary shared Favorites file");
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return std::unexpected("Could not write the temporary shared Favorites file");
        }
        output.close();

        if (!MoveFileExW(
                temporaryPath.c_str(), path_.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const auto error = GetLastError();
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return std::unexpected(WindowsError(
                "Could not replace the shared Favorites file", error));
        }
        return {};
    }

    std::expected<void, std::string> SharedFavoritesRepository::Clear() const
    {
        if (path_.empty()) return {};
        std::error_code error;
        const bool removed = std::filesystem::remove(path_, error);
        if (error) return std::unexpected("Could not remove the shared Favorites file: " + error.message());
        static_cast<void>(removed);
        const auto temporaryPath = std::filesystem::path(path_.wstring() + L".tmp");
        std::filesystem::remove(temporaryPath, error);
        if (error) return std::unexpected("Could not remove the temporary shared Favorites file: " + error.message());
        return {};
    }
}
