#include "Search/SearchText.h"
#include "Core/TextFold.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <system_error>

namespace whereabouts
{
    namespace
    {
        std::string Trim(std::string_view value)
        {
            const auto space = [](unsigned char character) {
                return std::isspace(character) != 0;
            };
            const auto first = std::find_if_not(value.begin(), value.end(), space);
            const auto last = std::find_if_not(value.rbegin(), value.rend(), space).base();
            return first < last ? std::string(first, last) : std::string{};
        }

        bool HasPluginExtension(std::string_view plugin)
        {
            if (plugin.size() < 4) return false;
            const auto extension = FoldTextForSearch(plugin.substr(plugin.size() - 4));
            return extension == ".esm" || extension == ".esp" || extension == ".esl";
        }

        std::optional<std::uint32_t> ParseHex(std::string_view value)
        {
            std::uint32_t parsed{};
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), parsed, 16);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
                return std::nullopt;
            }
            return parsed;
        }
    }

    std::expected<ParsedSearchText, SearchTextError> ParseSearchText(std::string_view text)
    {
        const auto value = Trim(text);
        auto numericText = std::string_view(value);
        const bool explicitHex = numericText.size() > 2 && numericText[0] == '0' &&
            (numericText[1] == 'x' || numericText[1] == 'X');
        if (explicitHex) {
            numericText.remove_prefix(2);
        }
        const bool containsDecimalDigit = std::ranges::any_of(numericText, [](char character) {
            return character >= '0' && character <= '9';
        });
        const bool numericIntent = explicitHex || containsDecimalDigit;
        if (numericIntent && !numericText.empty() && numericText.size() <= 8) {
            if (const auto formID = ParseHex(numericText)) {
                return ParsedSearchText{
                    .kind = SearchTextKind::NumericFormID,
                    .name = value,
                    .formID = *formID};
            }
        }

        const auto colon = value.rfind(':');
        if (colon != std::string::npos) {
            const auto plugin = Trim(std::string_view(value).substr(0, colon));
            const auto local = Trim(std::string_view(value).substr(colon + 1));
            if (HasPluginExtension(plugin)) {
                if (local.empty() || local.size() > 6) {
                    return std::unexpected(SearchTextError{
                        "Use Plugin.esm:localID with 1-6 hexadecimal digits"});
                }
                const auto localID = ParseHex(local);
                if (!localID) {
                    return std::unexpected(SearchTextError{
                        "Local FormID must contain only hexadecimal digits"});
                }
                return ParsedSearchText{
                    .kind = SearchTextKind::StableIdentity,
                    .name = value,
                    .identity = StableIdentityQuery{plugin, *localID}};
            }
        }

        return ParsedSearchText{.kind = SearchTextKind::Name, .name = value};
    }
}
