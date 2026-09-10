#pragma once

#include <format>
#include <string>
#include <string_view>

namespace whereabouts::ui
{
    [[nodiscard]] std::string FormatLocalizedTextArgs(
        std::string_view translatedFormat,
        std::string_view englishFormat,
        std::format_args args) noexcept;

    template <class... Args>
    [[nodiscard]] std::string FormatLocalizedText(
        std::string_view translatedFormat,
        std::string_view englishFormat,
        Args&&... args) noexcept
    {
        return FormatLocalizedTextArgs(
            translatedFormat,
            englishFormat,
            std::make_format_args(args...));
    }
}
