#include "UI/LocalizationResource.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <array>

namespace whereabouts::ui
{
    namespace
    {
        bool AppendUtf8(std::string& output, std::uint32_t codepoint)
        {
            if (codepoint <= 0x7F) {
                output.push_back(static_cast<char>(codepoint));
            } else if (codepoint <= 0x7FF) {
                output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else if (codepoint <= 0xFFFF) {
                if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return false;
                output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else if (codepoint <= 0x10FFFF) {
                output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else {
                return false;
            }
            return true;
        }

        std::expected<std::string, std::string> ToUtf8(std::u16string_view text)
        {
            std::string result;
            result.reserve(text.size());
            for (std::size_t index = 0; index < text.size(); ++index) {
                std::uint32_t codepoint = text[index];
                if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                    if (++index >= text.size()) return std::unexpected("truncated UTF-16 surrogate");
                    const auto low = static_cast<std::uint32_t>(text[index]);
                    if (low < 0xDC00 || low > 0xDFFF) {
                        return std::unexpected("invalid UTF-16 surrogate");
                    }
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                }
                if (!AppendUtf8(result, codepoint)) {
                    return std::unexpected("invalid UTF-16 codepoint");
                }
            }
            return result;
        }
    }

    std::expected<TranslationMap, std::string> ParseTranslationResource(
        std::span<const std::byte> bytes)
    {
        if (bytes.size() < 2 || bytes[0] != std::byte{0xFF} || bytes[1] != std::byte{0xFE}) {
            return std::unexpected("translation file is not UTF-16 LE with a BOM");
        }
        if ((bytes.size() - 2) % 2 != 0) {
            return std::unexpected("translation file has a truncated UTF-16 code unit");
        }

        std::u16string text;
        text.reserve((bytes.size() - 2) / 2);
        for (std::size_t index = 2; index < bytes.size(); index += 2) {
            const auto low = std::to_integer<std::uint8_t>(bytes[index]);
            const auto high = std::to_integer<std::uint8_t>(bytes[index + 1]);
            text.push_back(static_cast<char16_t>(low | (high << 8)));
        }

        TranslationMap translations;
        std::size_t lineStart = 0;
        while (lineStart <= text.size()) {
            const auto lineEnd = text.find(u'\n', lineStart);
            auto line = std::u16string_view{text}.substr(
                lineStart,
                lineEnd == std::u16string::npos ? text.size() - lineStart : lineEnd - lineStart);
            if (!line.empty() && line.back() == u'\r') line.remove_suffix(1);
            const auto delimiter = line.find(u'\t');
            if (delimiter >= 2 && delimiter != std::u16string_view::npos && line.front() == u'$') {
                const auto key = ToUtf8(line.substr(0, delimiter));
                const auto value = ToUtf8(line.substr(delimiter + 1));
                if (!key || !value) return std::unexpected(key ? value.error() : key.error());
                if (!value->empty()) translations.insert_or_assign(*key, *value);
            }
            if (lineEnd == std::u16string::npos) break;
            lineStart = lineEnd + 1;
        }
        return translations;
    }

    std::string NormalizeTranslationLanguage(std::string_view language)
    {
        if (language.empty() || language.size() > 24 ||
            !std::ranges::all_of(language, [](unsigned char value) {
                return std::isalpha(value) != 0;
            })) {
            return "ENGLISH";
        }
        std::string normalized{language};
        std::ranges::transform(normalized, normalized.begin(), [](unsigned char value) {
            return static_cast<char>(std::toupper(value));
        });
        constexpr std::array supported{
            std::string_view{"ENGLISH"}, std::string_view{"CHINESE"},
            std::string_view{"FRENCH"}, std::string_view{"GERMAN"},
            std::string_view{"ITALIAN"}, std::string_view{"JAPANESE"},
            std::string_view{"POLISH"}, std::string_view{"RUSSIAN"},
            std::string_view{"SPANISH"}};
        return std::ranges::find(supported, normalized) != supported.end() ?
            normalized : "ENGLISH";
    }

    std::string ResolveTranslationLanguage(
        std::string_view configuredLanguage,
        std::string_view gameLanguage)
    {
        std::string configured{configuredLanguage};
        std::ranges::transform(configured, configured.begin(), [](unsigned char value) {
            return static_cast<char>(std::toupper(value));
        });
        if (configured == "FOLLOW_SKYRIM" || configured == "FOLLOWSKYRIM" ||
            configured == "GAME") {
            return NormalizeTranslationLanguage(gameLanguage);
        }
        const auto normalized = NormalizeTranslationLanguage(configured);
        return normalized == "ENGLISH" && configured != "ENGLISH" ?
            NormalizeTranslationLanguage(gameLanguage) : normalized;
    }
}
