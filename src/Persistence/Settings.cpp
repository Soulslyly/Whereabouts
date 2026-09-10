#include "Persistence/Settings.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>

namespace whereabouts
{
    namespace
    {
        std::string Trim(std::string_view value)
        {
            const auto isSpace = [](unsigned char character) {
                return std::isspace(character) != 0;
            };
            const auto first = std::find_if_not(value.begin(), value.end(), isSpace);
            const auto last = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
            return first < last ? std::string(first, last) : std::string{};
        }

        std::string Lower(std::string_view value)
        {
            std::string result(value);
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            return result;
        }

        std::optional<bool> ParseBool(std::string_view text)
        {
            const auto value = Lower(Trim(text));
            if (value == "true" || value == "yes" || value == "on" || value == "1") {
                return true;
            }
            if (value == "false" || value == "no" || value == "off" || value == "0") {
                return false;
            }
            return std::nullopt;
        }

        template <class Number>
        std::optional<Number> ParseNumber(std::string_view text)
        {
            const auto value = Trim(text);
            Number result{};
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                return std::nullopt;
            }
            return result;
        }

        const char* BoolText(bool value)
        {
            return value ? "true" : "false";
        }

        std::optional<ControllerKeyboardLayout> ParseControllerKeyboardLayout(std::string_view text)
        {
            const auto value = Lower(Trim(text));
            if (value == "alphabetical") return ControllerKeyboardLayout::Alphabetical;
            if (value == "qwerty") return ControllerKeyboardLayout::Qwerty;
            return std::nullopt;
        }

        const char* ControllerKeyboardLayoutText(ControllerKeyboardLayout value)
        {
            return value == ControllerKeyboardLayout::Qwerty ? "QWERTY" : "Alphabetical";
        }

        std::optional<DistanceUnit> ParseDistanceUnit(std::string_view text)
        {
            const auto value = Lower(Trim(text));
            if (value == "meters") return DistanceUnit::Meters;
            if (value == "feet") return DistanceUnit::Feet;
            if (value == "gameunits") return DistanceUnit::GameUnits;
            return std::nullopt;
        }

        const char* DistanceUnitText(DistanceUnit value)
        {
            switch (value) {
            case DistanceUnit::Meters: return "Meters";
            case DistanceUnit::Feet: return "Feet";
            case DistanceUnit::GameUnits: return "GameUnits";
            }
            return "Meters";
        }

        std::optional<ResultDensity> ParseResultDensity(std::string_view text)
        {
            const auto value = Lower(Trim(text));
            if (value == "detailed") return ResultDensity::Detailed;
            if (value == "compact") return ResultDensity::Compact;
            return std::nullopt;
        }

        const char* ResultDensityText(ResultDensity value)
        {
            switch (value) {
            case ResultDensity::Detailed: return "Detailed";
            case ResultDensity::Compact: return "Compact";
            }
            return "Detailed";
        }

        std::optional<TranslationLanguage> ParseTranslationLanguage(std::string_view text)
        {
            const auto value = Lower(Trim(text));
            if (value == "followskyrim" || value == "game") return TranslationLanguage::FollowSkyrim;
            if (value == "english") return TranslationLanguage::English;
            if (value == "chinese") return TranslationLanguage::Chinese;
            if (value == "french") return TranslationLanguage::French;
            if (value == "german") return TranslationLanguage::German;
            if (value == "italian") return TranslationLanguage::Italian;
            if (value == "japanese") return TranslationLanguage::Japanese;
            if (value == "polish") return TranslationLanguage::Polish;
            if (value == "russian") return TranslationLanguage::Russian;
            if (value == "spanish") return TranslationLanguage::Spanish;
            return std::nullopt;
        }

        bool ApplySetting(Settings& settings, std::string_view section, std::string_view key, std::string_view value)
        {
            const auto path = Lower(section) + "." + Lower(key);
            const auto boolean = [&]() { return ParseBool(value); };
            const auto integer = [&]() { return ParseNumber<std::uint32_t>(value); };

#define WHEREABOUTS_BOOL(settingPath, field) \
            if (path == settingPath) { const auto parsed = boolean(); if (!parsed) return false; settings.field = *parsed; return true; }
#define WHEREABOUTS_UINT(settingPath, field) \
            if (path == settingPath) { const auto parsed = integer(); if (!parsed) return false; settings.field = *parsed; return true; }

            if (path == "general.enabled") return true;
            WHEREABOUTS_BOOL("general.livesearch", liveSearch)
            if (path == "general.regexdefault") return true;
            WHEREABOUTS_UINT("general.recentlimit", recentLimit)
            WHEREABOUTS_BOOL("general.debuglogging", debugLogging)

            if (path == "targeting.teleportrange") {
                const auto parsed = ParseNumber<float>(value);
                if (!parsed || !std::isfinite(*parsed)) return false;
                settings.teleportRange = *parsed;
                return true;
            }
            if (path == "targeting.preferconsoletarget") return true;
            WHEREABOUTS_BOOL("targeting.autoselectconsoletarget", autoSelectConsoleTarget)
            WHEREABOUTS_BOOL("targeting.autoselectcrosshairtarget", autoSelectCrosshairTarget)
            WHEREABOUTS_BOOL("targeting.crosshairfallback", autoSelectCrosshairTarget)
            WHEREABOUTS_BOOL("tracking.notifytrackeddeath", notifyTrackedDeath)
            if (path == "tracking.removetrackingondeath") return true;

            WHEREABOUTS_BOOL("safety.confirmdisable", confirmDisable)
            WHEREABOUTS_BOOL("safety.confirmteleport", confirmTeleport)

            WHEREABOUTS_BOOL("interface.keepopenafterinlinecommand", keepOpenAfterInlineCommand)
            WHEREABOUTS_BOOL("interface.rememberfilters", rememberFilters)
            if (path == "interface.controllerkeyboardlayout") {
                const auto parsed = ParseControllerKeyboardLayout(value);
                if (!parsed) return false;
                settings.controllerKeyboardLayout = *parsed;
                return true;
            }
            if (path == "interface.distanceunit") {
                const auto parsed = ParseDistanceUnit(value);
                if (!parsed) return false;
                settings.distanceUnit = *parsed;
                return true;
            }
            if (path == "interface.resultdensity") {
                const auto parsed = ParseResultDensity(value);
                if (!parsed) return false;
                settings.resultDensity = *parsed;
                return true;
            }
            if (path == "interface.language") {
                const auto parsed = ParseTranslationLanguage(value);
                if (!parsed) return false;
                settings.translationLanguage = *parsed;
                return true;
            }
            WHEREABOUTS_UINT("interface.liveresultlimit", liveResultLimit)
            WHEREABOUTS_UINT("interface.fullresultlimit", fullResultLimit)
            WHEREABOUTS_BOOL("interface.showallresults", showAllResults)
            if (path == "interface.copyidformat") return true;
#undef WHEREABOUTS_UINT
#undef WHEREABOUTS_BOOL
            return false;
        }

        bool IsKnownSetting(std::string_view section, std::string_view key)
        {
            const auto path = Lower(section) + "." + Lower(key);
            if (path == "interface.controllerkeyboardlayout" ||
                path == "interface.distanceunit" ||
                path == "interface.resultdensity" ||
                path == "interface.language" ||
                path == "interface.copyidformat" ||
                path == "general.enabled") {
                return true;
            }
            Settings probe;
            return ApplySetting(probe, section, key, "0") ||
                   ApplySetting(probe, section, key, "false") ||
                   ApplySetting(probe, section, key, "Name");
        }

        std::string DescribeWindowsError(DWORD code)
        {
            return "Windows error " + std::to_string(code);
        }
    }

    const char* TranslationLanguageSettingText(TranslationLanguage language) noexcept
    {
        switch (language) {
        case TranslationLanguage::FollowSkyrim: return "FollowSkyrim";
        case TranslationLanguage::English: return "English";
        case TranslationLanguage::Chinese: return "Chinese";
        case TranslationLanguage::French: return "French";
        case TranslationLanguage::German: return "German";
        case TranslationLanguage::Italian: return "Italian";
        case TranslationLanguage::Japanese: return "Japanese";
        case TranslationLanguage::Polish: return "Polish";
        case TranslationLanguage::Russian: return "Russian";
        case TranslationLanguage::Spanish: return "Spanish";
        }
        return "FollowSkyrim";
    }

    void Settings::Normalize() noexcept
    {
        teleportRange = std::isfinite(teleportRange) ?
            std::clamp(teleportRange, 0.0F, 1000.0F) : 100.0F;
        recentLimit = std::clamp(recentLimit, 1U, 100U);
        liveResultLimit = std::clamp(liveResultLimit, 10U, 100U);
        fullResultLimit = std::clamp(fullResultLimit, 50U, 1000U);
        if (controllerKeyboardLayout != ControllerKeyboardLayout::Alphabetical &&
            controllerKeyboardLayout != ControllerKeyboardLayout::Qwerty) {
            controllerKeyboardLayout = ControllerKeyboardLayout::Alphabetical;
        }
        if (distanceUnit != DistanceUnit::Meters &&
            distanceUnit != DistanceUnit::Feet &&
            distanceUnit != DistanceUnit::GameUnits) {
            distanceUnit = DistanceUnit::Meters;
        }
        if (resultDensity != ResultDensity::Detailed &&
            resultDensity != ResultDensity::Compact) {
            resultDensity = ResultDensity::Detailed;
        }
        if (translationLanguage < TranslationLanguage::FollowSkyrim ||
            translationLanguage > TranslationLanguage::Spanish) {
            translationLanguage = TranslationLanguage::FollowSkyrim;
        }
    }

    SettingsRepository::SettingsRepository(std::filesystem::path path) :
        path_(std::move(path))
    {}

    SettingsLoadResult SettingsRepository::Load() const
    {
        SettingsLoadResult result;
        std::ifstream input(path_);
        if (!input) {
            return result;
        }

        std::string section;
        std::string line;
        bool sawUnknown = false;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            const auto trimmed = Trim(line);
            if (trimmed.empty() || trimmed.starts_with(';') || trimmed.starts_with('#')) {
                continue;
            }
            if (trimmed.front() == '[' && trimmed.back() == ']') {
                section = Trim(std::string_view(trimmed).substr(1, trimmed.size() - 2));
                continue;
            }

            const auto equals = trimmed.find('=');
            if (equals == std::string::npos) {
                result.warnings.push_back("Malformed setting on line " + std::to_string(lineNumber));
                continue;
            }
            const auto key = Trim(std::string_view(trimmed).substr(0, equals));
            const auto value = Trim(std::string_view(trimmed).substr(equals + 1));
            if (!IsKnownSetting(section, key)) {
                sawUnknown = true;
                continue;
            }
            if (!ApplySetting(result.settings, section, key, value)) {
                result.warnings.push_back("Invalid value for [" + section + "] " + key);
            }
        }

        if (sawUnknown) {
            result.warnings.push_back("One or more unknown settings were ignored");
        }
        result.settings.Normalize();
        return result;
    }

    std::expected<void, std::string> SettingsRepository::Save(const Settings& source) const
    {
        auto settings = source;
        settings.Normalize();
        const auto temporaryPath = std::filesystem::path(path_.wstring() + L".tmp");

        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            return std::unexpected("Could not open the temporary settings file");
        }

        output << "[General]\n"
               << "LiveSearch=" << BoolText(settings.liveSearch) << '\n'
               << "RecentLimit=" << settings.recentLimit << '\n'
               << "DebugLogging=" << BoolText(settings.debugLogging) << "\n\n"
               << "[Targeting]\n"
               << "TeleportRange=" << settings.teleportRange << '\n'
               << "AutoSelectConsoleTarget=" << BoolText(settings.autoSelectConsoleTarget) << '\n'
               << "AutoSelectCrosshairTarget=" << BoolText(settings.autoSelectCrosshairTarget) << "\n\n"
               << "[Tracking]\n"
               << "NotifyTrackedDeath=" << BoolText(settings.notifyTrackedDeath) << "\n\n"
               << "[Safety]\n"
               << "ConfirmDisable=" << BoolText(settings.confirmDisable) << '\n'
               << "ConfirmTeleport=" << BoolText(settings.confirmTeleport) << "\n\n"
               << "[Interface]\n"
               << "KeepOpenAfterInlineCommand=" << BoolText(settings.keepOpenAfterInlineCommand) << '\n'
               << "RememberFilters=" << BoolText(settings.rememberFilters) << '\n'
               << "ControllerKeyboardLayout=" <<
                    ControllerKeyboardLayoutText(settings.controllerKeyboardLayout) << '\n'
               << "DistanceUnit=" << DistanceUnitText(settings.distanceUnit) << '\n'
               << "ResultDensity=" << ResultDensityText(settings.resultDensity) << '\n'
               << "Language=" << TranslationLanguageSettingText(settings.translationLanguage) << '\n'
               << "LiveResultLimit=" << settings.liveResultLimit << '\n'
               << "FullResultLimit=" << settings.fullResultLimit << '\n'
               << "ShowAllResults=" << BoolText(settings.showAllResults) << '\n';
        output.flush();
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return std::unexpected("Could not write the temporary settings file");
        }
        output.close();

        if (!MoveFileExW(
                temporaryPath.c_str(),
                path_.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const auto error = GetLastError();
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return std::unexpected("Could not replace the settings file: " + DescribeWindowsError(error));
        }
        return {};
    }
}
