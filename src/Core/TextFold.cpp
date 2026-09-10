#include "Core/TextFold.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace whereabouts
{
    namespace
    {
        struct FoldResult
        {
            std::string text;
        };

        [[nodiscard]] constexpr char FoldAscii(char value) noexcept
        {
            return value >= 'A' && value <= 'Z' ?
                static_cast<char>(value + ('a' - 'A')) : value;
        }

        [[nodiscard]] FoldResult Fold(std::string_view value)
        {
            const auto asciiFallback = [&value] {
                std::string result(value);
                std::ranges::transform(result, result.begin(), FoldAscii);
                return FoldResult{std::move(result)};
            };

            if (value.empty()) return {{}};
            if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
                return asciiFallback();
            }

            const auto sourceLength = static_cast<int>(value.size());
            const auto wideLength = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), sourceLength, nullptr, 0);
            if (wideLength <= 0) return asciiFallback();

            std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
            if (MultiByteToWideChar(
                    CP_UTF8,
                    MB_ERR_INVALID_CHARS,
                    value.data(),
                    sourceLength,
                    wide.data(),
                    wideLength) != wideLength) {
                return asciiFallback();
            }

            const auto lowerLength = LCMapStringEx(
                LOCALE_NAME_INVARIANT,
                LCMAP_LOWERCASE,
                wide.data(),
                wideLength,
                nullptr,
                0,
                nullptr,
                nullptr,
                0);
            if (lowerLength <= 0) return asciiFallback();

            std::wstring lower(static_cast<std::size_t>(lowerLength), L'\0');
            if (LCMapStringEx(
                    LOCALE_NAME_INVARIANT,
                    LCMAP_LOWERCASE,
                    wide.data(),
                    wideLength,
                    lower.data(),
                    lowerLength,
                    nullptr,
                    nullptr,
                    0) != lowerLength) {
                return asciiFallback();
            }

            const auto utf8Length = WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                lower.data(),
                lowerLength,
                nullptr,
                0,
                nullptr,
                nullptr);
            if (utf8Length <= 0) return asciiFallback();

            std::string result(static_cast<std::size_t>(utf8Length), '\0');
            if (WideCharToMultiByte(
                    CP_UTF8,
                    WC_ERR_INVALID_CHARS,
                    lower.data(),
                    lowerLength,
                    result.data(),
                    utf8Length,
                    nullptr,
                    nullptr) != utf8Length) {
                return asciiFallback();
            }
            return {std::move(result)};
        }

        [[nodiscard]] bool IsCodePointBoundary(std::string_view value, std::size_t offset) noexcept
        {
            if (offset == 0 || offset == value.size()) return true;
            const auto byte = static_cast<unsigned char>(value[offset]);
            return (byte & 0xC0U) != 0x80U;
        }

        [[nodiscard]] bool EqualsAsciiNoAllocation(
            std::string_view left,
            std::string_view right) noexcept
        {
            if (left.size() != right.size()) return false;
            for (std::size_t index = 0; index < left.size(); ++index) {
                if (FoldAscii(left[index]) != FoldAscii(right[index])) return false;
            }
            return true;
        }
    }

    std::string FoldTextForSearch(std::string_view value)
    {
        return Fold(value).text;
    }

    bool SearchTextEquals(std::string_view left, std::string_view right)
    {
        return Fold(left).text == Fold(right).text;
    }

    bool SearchTextEqualsNoexcept(std::string_view left, std::string_view right) noexcept
    {
        try {
            return SearchTextEquals(left, right);
        } catch (...) {
            return EqualsAsciiNoAllocation(left, right);
        }
    }

    bool SearchTextStartsWith(std::string_view value, std::string_view prefix)
    {
        const auto foldedValue = Fold(value);
        const auto foldedPrefix = Fold(prefix);
        return foldedValue.text.starts_with(foldedPrefix.text) &&
               IsCodePointBoundary(foldedValue.text, foldedPrefix.text.size());
    }

    bool SearchTextContains(std::string_view value, std::string_view fragment)
    {
        const auto foldedValue = Fold(value);
        const auto foldedFragment = Fold(fragment);
        if (foldedFragment.text.empty()) return true;

        auto position = foldedValue.text.find(foldedFragment.text);
        while (position != std::string::npos) {
            const auto end = position + foldedFragment.text.size();
            if (IsCodePointBoundary(foldedValue.text, position) &&
                IsCodePointBoundary(foldedValue.text, end)) {
                return true;
            }
            position = foldedValue.text.find(foldedFragment.text, position + 1);
        }
        return false;
    }

    int CompareSearchText(std::string_view left, std::string_view right)
    {
        return Fold(left).text.compare(Fold(right).text);
    }
}
