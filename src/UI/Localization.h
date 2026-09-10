#pragma once

#include <array>
#include <format>
#include <span>
#include <string>
#include <string_view>

#include "UI/LocalizationFormat.h"
#include "Persistence/Settings.h"
#include "Core/SpatialPresentation.h"

namespace whereabouts::ui
{
    struct TranslationEntry
    {
        std::string_view key;
        std::string_view english;
    };

    inline constexpr auto kTranslationCatalog = std::to_array<TranslationEntry>({
#define WA_TEXT(name, english) TranslationEntry{"$Whereabouts_" #name, english},
#include "UI/TranslationCatalog.inc"
#undef WA_TEXT
    });

    [[nodiscard]] bool InitializeLocalization(TranslationLanguage language) noexcept;
    [[nodiscard]] std::string ResolveConfiguredTranslationLanguage(
        TranslationLanguage language) noexcept;
    [[nodiscard]] std::string LoadedTranslationLanguage() noexcept;
    [[nodiscard]] const char* TranslateText(const char* english) noexcept;
    [[nodiscard]] std::string TranslateOwned(std::string_view english) noexcept;
    [[nodiscard]] inline std::string LocalizedPrimarySpatialLabel(const SpatialSnapshot& spatial)
    {
        const auto label = PrimarySpatialLabel(spatial);
        return spatial.location.empty() && spatial.cell.empty() && spatial.worldspace.empty() ?
            TranslateOwned(label) : label;
    }
    [[nodiscard]] std::string TranslateFormatArgs(
        const char* englishFormat,
        std::format_args args) noexcept;

    template <class... Args>
    [[nodiscard]] std::string TranslateFormat(const char* englishFormat, Args&&... args) noexcept
    {
        return TranslateFormatArgs(englishFormat, std::make_format_args(args...));
    }
}
