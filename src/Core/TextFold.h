#pragma once

#include <string>
#include <string_view>

namespace whereabouts
{
    [[nodiscard]] std::string FoldTextForSearch(std::string_view value);
    [[nodiscard]] bool SearchTextEquals(std::string_view left, std::string_view right);
    [[nodiscard]] bool SearchTextEqualsNoexcept(
        std::string_view left,
        std::string_view right) noexcept;
    [[nodiscard]] bool SearchTextStartsWith(std::string_view value, std::string_view prefix);
    [[nodiscard]] bool SearchTextContains(std::string_view value, std::string_view fragment);
    [[nodiscard]] int CompareSearchText(std::string_view left, std::string_view right);
}
