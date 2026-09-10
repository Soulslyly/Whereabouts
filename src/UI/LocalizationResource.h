#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace whereabouts::ui
{
    using TranslationMap = std::unordered_map<std::string, std::string>;

    [[nodiscard]] std::expected<TranslationMap, std::string>
        ParseTranslationResource(std::span<const std::byte> bytes);
    [[nodiscard]] std::string NormalizeTranslationLanguage(std::string_view language);
    [[nodiscard]] std::string ResolveTranslationLanguage(
        std::string_view configuredLanguage,
        std::string_view gameLanguage);
}
