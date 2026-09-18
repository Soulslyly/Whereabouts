#include "Persistence/Settings.h"
#include "Commands/CommandActions.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
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

        std::string NormalizeActionID(std::string_view text)
        {
            auto value = Lower(Trim(text));
            if (value.empty()) return std::string(kDisabledActionID);
            if (!std::ranges::all_of(value, [](unsigned char character) {
                    return std::isalnum(character) != 0 ||
                        character == '.' || character == '_' || character == '-';
                })) {
                return std::string(kDisabledActionID);
            }
            return value;
        }

        bool HasControlCharacters(std::string_view text)
        {
            return std::ranges::any_of(text, [](unsigned char character) {
                return std::iscntrl(character) != 0;
            });
        }

        std::optional<std::size_t> CustomCommandIndex(std::string_view actionID)
        {
            constexpr std::string_view prefix = "custom.";
            if (!actionID.starts_with(prefix)) return std::nullopt;
            const auto number = ParseNumber<std::size_t>(actionID.substr(prefix.size()));
            if (!number || *number == 0 || *number > kCustomCommandSlotCount) {
                return std::nullopt;
            }
            return *number - 1;
        }

        std::vector<std::string> ParseActionList(std::string_view text)
        {
            std::vector<std::string> result;
            std::size_t first = 0;
            while (first <= text.size()) {
                const auto comma = text.find(',', first);
                const auto end = comma == std::string_view::npos ? text.size() : comma;
                result.push_back(std::string(text.substr(first, end - first)));
                if (comma == std::string_view::npos) break;
                first = comma + 1;
            }
            return result;
        }

        std::string JoinActionList(const std::vector<std::string>& values)
        {
            std::ostringstream output;
            for (std::size_t index = 0; index < values.size(); ++index) {
                if (index != 0) output << ',';
                output << values[index];
            }
            return output.str();
        }

        std::optional<std::size_t> CustomCommandSectionIndex(std::string_view section)
        {
            constexpr std::string_view prefix = "customcommand";
            const auto lower = Lower(Trim(section));
            if (!lower.starts_with(prefix)) return std::nullopt;
            const auto number = ParseNumber<std::size_t>(
                std::string_view(lower).substr(prefix.size()));
            if (!number || *number == 0 || *number > kCustomCommandSlotCount) {
                return std::nullopt;
            }
            return *number - 1;
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
            if (value == "supercompact") return ResultDensity::SuperCompact;
            return std::nullopt;
        }

        const char* ResultDensityText(ResultDensity value)
        {
            switch (value) {
            case ResultDensity::Detailed: return "Detailed";
            case ResultDensity::Compact: return "Compact";
            case ResultDensity::SuperCompact: return "SuperCompact";
            }
            return "Detailed";
        }

        std::optional<DensityOverride> ParseDensityOverride(std::string_view text)
        {
            const auto value = Lower(Trim(text));
            if (value == "useglobal") return DensityOverride::UseGlobal;
            if (value == "detailed") return DensityOverride::Detailed;
            if (value == "compact") return DensityOverride::Compact;
            if (value == "supercompact") return DensityOverride::SuperCompact;
            return std::nullopt;
        }

        const char* DensityOverrideText(DensityOverride value)
        {
            switch (value) {
            case DensityOverride::UseGlobal: return "UseGlobal";
            case DensityOverride::Detailed: return "Detailed";
            case DensityOverride::Compact: return "Compact";
            case DensityOverride::SuperCompact: return "SuperCompact";
            }
            return "UseGlobal";
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
            const auto keyLower = Lower(key);
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
            WHEREABOUTS_BOOL("general.sharefavoritesbetweensaves", shareFavoritesAcrossSaves)

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

            WHEREABOUTS_BOOL("commands.showconfirmations", showCommandConfirmations)
            WHEREABOUTS_UINT("commands.examplesversion", customCommandExamplesVersion)
            if (path == "commands.rowbuttonaction") {
                settings.rowButtonAction = std::string(value);
                return true;
            }
            if (path == "commands.doubleclickaction") {
                settings.doubleClickAction = std::string(value);
                return true;
            }
            if (path == "commands.order") {
                settings.commandOrder = ParseActionList(value);
                return true;
            }
            if (path == "commands.hidden") {
                settings.hiddenCommands = ParseActionList(value);
                return true;
            }
            if (const auto slot = CustomCommandSectionIndex(section)) {
                auto& custom = settings.customCommands[*slot];
                if (keyLower == "enabled") {
                    const auto parsed = boolean();
                    if (!parsed) return false;
                    custom.enabled = *parsed;
                    return true;
                }
                if (keyLower == "name") {
                    custom.name = std::string(value);
                    return true;
                }
                if (keyLower == "command") {
                    custom.command = std::string(value);
                    return true;
                }
            }

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
#define WHEREABOUTS_DENSITY_OVERRIDE(settingPath, field) \
            if (path == settingPath) { const auto parsed = ParseDensityOverride(value); if (!parsed) return false; settings.field = *parsed; return true; }
            WHEREABOUTS_DENSITY_OVERRIDE("interface.searchdensity", searchDensity)
            WHEREABOUTS_DENSITY_OVERRIDE("interface.filterdensity", filterDensity)
            WHEREABOUTS_DENSITY_OVERRIDE("interface.advancedfilterdensity", advancedFilterDensity)
            WHEREABOUTS_DENSITY_OVERRIDE("interface.resultsdensity", resultsDensity)
            WHEREABOUTS_DENSITY_OVERRIDE("interface.selectednpcdensity", selectedNpcDensity)
            WHEREABOUTS_DENSITY_OVERRIDE("interface.savedlistsdensity", savedListsDensity)
            WHEREABOUTS_DENSITY_OVERRIDE("interface.inspectordensity", inspectorDensity)
#undef WHEREABOUTS_DENSITY_OVERRIDE
            if (path == "interface.language") {
                const auto parsed = ParseTranslationLanguage(value);
                if (!parsed) return false;
                settings.translationLanguage = *parsed;
                return true;
            }
            WHEREABOUTS_UINT("interface.liveresultlimit", liveResultLimit)
            WHEREABOUTS_UINT("interface.fullresultlimit", fullResultLimit)
            WHEREABOUTS_BOOL("interface.showallresults", showAllResults)
            WHEREABOUTS_BOOL("interface.enableadvancedfilters", enableAdvancedFilters)
            WHEREABOUTS_BOOL("interface.showactivefilternames", showActiveFilterNames)
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
                path == "interface.searchdensity" ||
                path == "interface.filterdensity" ||
                path == "interface.advancedfilterdensity" ||
                path == "interface.resultsdensity" ||
                path == "interface.selectednpcdensity" ||
                path == "interface.savedlistsdensity" ||
                path == "interface.inspectordensity" ||
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

    ResultDensity Settings::DensityFor(UiDensityArea area) const noexcept
    {
        DensityOverride overrideValue = DensityOverride::UseGlobal;
        switch (area) {
        case UiDensityArea::Search: overrideValue = searchDensity; break;
        case UiDensityArea::Filters: overrideValue = filterDensity; break;
        case UiDensityArea::AdvancedFilters: overrideValue = advancedFilterDensity; break;
        case UiDensityArea::Results: overrideValue = resultsDensity; break;
        case UiDensityArea::SelectedNpc: overrideValue = selectedNpcDensity; break;
        case UiDensityArea::SavedLists: overrideValue = savedListsDensity; break;
        case UiDensityArea::Inspector: overrideValue = inspectorDensity; break;
        }
        switch (overrideValue) {
        case DensityOverride::Detailed: return ResultDensity::Detailed;
        case DensityOverride::Compact: return ResultDensity::Compact;
        case DensityOverride::SuperCompact: return ResultDensity::SuperCompact;
        case DensityOverride::UseGlobal: return resultDensity;
        }
        return resultDensity;
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
            resultDensity != ResultDensity::Compact &&
            resultDensity != ResultDensity::SuperCompact) {
            resultDensity = ResultDensity::Detailed;
        }
        const auto normalizeDensityOverride = [](DensityOverride& value) {
            if (value < DensityOverride::UseGlobal || value > DensityOverride::SuperCompact) {
                value = DensityOverride::UseGlobal;
            }
        };
        normalizeDensityOverride(searchDensity);
        normalizeDensityOverride(filterDensity);
        normalizeDensityOverride(advancedFilterDensity);
        normalizeDensityOverride(resultsDensity);
        normalizeDensityOverride(selectedNpcDensity);
        normalizeDensityOverride(savedListsDensity);
        normalizeDensityOverride(inspectorDensity);
        if (translationLanguage < TranslationLanguage::FollowSkyrim ||
            translationLanguage > TranslationLanguage::Spanish) {
            translationLanguage = TranslationLanguage::FollowSkyrim;
        }

        for (auto& custom : customCommands) {
            custom.name = Trim(custom.name);
            custom.command = Trim(custom.command);
            if (HasControlCharacters(custom.name)) custom.name.clear();
            if (HasControlCharacters(custom.command)) custom.command.clear();
        }

        if (customCommandExamplesVersion < 1) {
            constexpr std::array examples{
                std::pair{"Reset AI", "resetai"},
                std::pair{"Unequip Everything", "unequipall"},
                std::pair{"Restore Health", "restoreav health 1000"},
                std::pair{"Kill", "kill"}};
            for (std::size_t index = 0; index < examples.size(); ++index) {
                auto& slot = customCommands[index];
                if (slot.name.empty() && slot.command.empty()) {
                    slot.enabled = false;
                    slot.name = examples[index].first;
                    slot.command = examples[index].second;
                }
            }
            customCommandExamplesVersion = 1;
        }

        for (auto& custom : customCommands) {
            custom.enabled = custom.enabled &&
                ValidateCustomCommand(custom.name, custom.command).has_value();
        }

        const auto actionAvailable = [&](std::string_view actionID) {
            const auto slot = CustomCommandIndex(actionID);
            if (actionID.starts_with("custom.")) {
                return slot && customCommands[*slot].enabled;
            }
            return actionID == kDisabledActionID ||
                actionID == kCopyNpcReportActionID ||
                CommandForActionID(actionID).has_value();
        };
        const auto normalizeAssignment = [&](std::string& actionID) {
            actionID = NormalizeActionID(actionID);
            if (!actionAvailable(actionID)) {
                actionID = std::string(kDisabledActionID);
            }
        };
        normalizeAssignment(rowButtonAction);
        normalizeAssignment(doubleClickAction);

        const auto layoutActionAvailable = [&](std::string_view actionID) {
            if (actionID == kActorFlagsGroupActionID || actionID == kCopyIdentityActionID) {
                return true;
            }
            return actionAvailable(actionID) && !IsActorFlagActionID(actionID);
        };
        const auto normalizeLayoutList = [&](std::vector<std::string>& values) {
            std::vector<std::string> normalized;
            normalized.reserve(values.size());
            for (const auto& raw : values) {
                auto actionID = NormalizeActionID(raw);
                if (IsActorFlagActionID(actionID)) {
                    actionID = std::string(kActorFlagsGroupActionID);
                }
                if (actionID == kDisabledActionID || !layoutActionAvailable(actionID) ||
                    std::ranges::find(normalized, actionID) != normalized.end()) {
                    continue;
                }
                normalized.push_back(std::move(actionID));
            }
            values = std::move(normalized);
        };
        normalizeLayoutList(commandOrder);

        const auto containsHidden = [&](std::string_view wanted) {
            return std::ranges::any_of(hiddenCommands, [&](const auto& raw) {
                return NormalizeActionID(raw) == wanted;
            });
        };
        const bool allLegacyActorFlagsHidden = std::ranges::all_of(
            kBuiltInCommandActions,
            [&](const auto& action) {
                return !IsActorFlagActionID(action.id) || containsHidden(action.id);
            });
        const bool hideActorFlags =
            containsHidden(kActorFlagsGroupActionID) || allLegacyActorFlagsHidden;
        std::erase_if(hiddenCommands, [](const auto& raw) {
            const auto actionID = NormalizeActionID(raw);
            return actionID == kActorFlagsGroupActionID || IsActorFlagActionID(actionID);
        });
        normalizeLayoutList(hiddenCommands);
        if (hideActorFlags) hiddenCommands.emplace_back(kActorFlagsGroupActionID);
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
               << "ShareFavoritesBetweenSaves=" << BoolText(settings.shareFavoritesAcrossSaves) << '\n'
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
               << "[Commands]\n"
               << "ShowConfirmations=" << BoolText(settings.showCommandConfirmations) << '\n'
               << "ExamplesVersion=" << settings.customCommandExamplesVersion << '\n'
               << "RowButtonAction=" << settings.rowButtonAction << '\n'
               << "DoubleClickAction=" << settings.doubleClickAction << '\n'
               << "Order=" << JoinActionList(settings.commandOrder) << '\n'
               << "Hidden=" << JoinActionList(settings.hiddenCommands) << "\n\n"
               << "[Interface]\n"
               << "KeepOpenAfterInlineCommand=" << BoolText(settings.keepOpenAfterInlineCommand) << '\n'
               << "RememberFilters=" << BoolText(settings.rememberFilters) << '\n'
               << "ControllerKeyboardLayout=" <<
                    ControllerKeyboardLayoutText(settings.controllerKeyboardLayout) << '\n'
               << "DistanceUnit=" << DistanceUnitText(settings.distanceUnit) << '\n'
               << "ResultDensity=" << ResultDensityText(settings.resultDensity) << '\n'
               << "SearchDensity=" << DensityOverrideText(settings.searchDensity) << '\n'
               << "FilterDensity=" << DensityOverrideText(settings.filterDensity) << '\n'
               << "AdvancedFilterDensity=" << DensityOverrideText(settings.advancedFilterDensity) << '\n'
               << "ResultsDensity=" << DensityOverrideText(settings.resultsDensity) << '\n'
               << "SelectedNpcDensity=" << DensityOverrideText(settings.selectedNpcDensity) << '\n'
               << "SavedListsDensity=" << DensityOverrideText(settings.savedListsDensity) << '\n'
               << "InspectorDensity=" << DensityOverrideText(settings.inspectorDensity) << '\n'
               << "Language=" << TranslationLanguageSettingText(settings.translationLanguage) << '\n'
               << "LiveResultLimit=" << settings.liveResultLimit << '\n'
               << "FullResultLimit=" << settings.fullResultLimit << '\n'
               << "ShowAllResults=" << BoolText(settings.showAllResults) << '\n'
               << "EnableAdvancedFilters=" << BoolText(settings.enableAdvancedFilters) << '\n'
               << "ShowActiveFilterNames=" << BoolText(settings.showActiveFilterNames) << '\n';
        for (std::size_t index = 0; index < settings.customCommands.size(); ++index) {
            const auto& custom = settings.customCommands[index];
            output << "\n[CustomCommand" << (index + 1) << "]\n"
                   << "Enabled=" << BoolText(custom.enabled) << '\n'
                   << "Name=" << custom.name << '\n'
                   << "Command=" << custom.command << '\n';
        }
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
