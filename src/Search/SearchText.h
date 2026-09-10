#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace whereabouts
{
    enum class SearchTextKind
    {
        Name,
        NumericFormID,
        StableIdentity
    };

    struct StableIdentityQuery
    {
        std::string plugin;
        std::uint32_t localID{0};
    };

    struct ParsedSearchText
    {
        SearchTextKind kind{SearchTextKind::Name};
        std::string name;
        std::uint32_t formID{0};
        std::optional<StableIdentityQuery> identity;
    };

    struct SearchTextError
    {
        std::string message;
    };

    [[nodiscard]] std::expected<ParsedSearchText, SearchTextError> ParseSearchText(
        std::string_view text);
}
