#include "UI/LocalizationFormat.h"

namespace whereabouts::ui
{
    std::string FormatLocalizedTextArgs(
        std::string_view translatedFormat,
        std::string_view englishFormat,
        std::format_args args) noexcept
    {
        try {
            return std::vformat(translatedFormat, args);
        } catch (...) {
            try {
                return std::vformat(englishFormat, args);
            } catch (...) {
                return std::string{englishFormat};
            }
        }
    }
}
