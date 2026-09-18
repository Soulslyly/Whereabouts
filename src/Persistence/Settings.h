#pragma once

#include "Core/DistanceFormat.h"
#include "Search/SearchEngine.h"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace whereabouts
{
    inline constexpr std::size_t kCustomCommandSlotCount = 8;
    inline constexpr std::string_view kDisabledActionID = "off";

    struct CustomCommandSettings
    {
        bool enabled{false};
        std::string name;
        std::string command;
    };

    enum class ControllerKeyboardLayout
    {
        Alphabetical,
        Qwerty
    };

    enum class ResultDensity
    {
        Detailed,
        Compact,
        SuperCompact
    };

    enum class DensityOverride
    {
        UseGlobal,
        Detailed,
        Compact,
        SuperCompact
    };

    enum class UiDensityArea
    {
        Search,
        Filters,
        AdvancedFilters,
        Results,
        SelectedNpc,
        SavedLists,
        Inspector
    };

    enum class TranslationLanguage
    {
        FollowSkyrim,
        English,
        Chinese,
        French,
        German,
        Italian,
        Japanese,
        Polish,
        Russian,
        Spanish
    };

    [[nodiscard]] const char* TranslationLanguageSettingText(
        TranslationLanguage language) noexcept;

    struct Settings
    {
        bool liveSearch{true};
        float teleportRange{100.0F};
        bool autoSelectConsoleTarget{false};
        bool autoSelectCrosshairTarget{false};
        bool notifyTrackedDeath{true};
        bool confirmDisable{true};
        bool confirmTeleport{false};
        bool keepOpenAfterInlineCommand{true};
        bool rememberFilters{true};
        bool debugLogging{false};
        std::uint32_t recentLimit{20};
        ControllerKeyboardLayout controllerKeyboardLayout{ControllerKeyboardLayout::Alphabetical};
        DistanceUnit distanceUnit{DistanceUnit::Meters};
        ResultDensity resultDensity{ResultDensity::Detailed};
        DensityOverride searchDensity{DensityOverride::UseGlobal};
        DensityOverride filterDensity{DensityOverride::UseGlobal};
        DensityOverride advancedFilterDensity{DensityOverride::UseGlobal};
        DensityOverride resultsDensity{DensityOverride::UseGlobal};
        DensityOverride selectedNpcDensity{DensityOverride::UseGlobal};
        DensityOverride savedListsDensity{DensityOverride::UseGlobal};
        DensityOverride inspectorDensity{DensityOverride::UseGlobal};
        TranslationLanguage translationLanguage{TranslationLanguage::FollowSkyrim};
        std::uint32_t liveResultLimit{50};
        std::uint32_t fullResultLimit{500};
        bool showAllResults{true};
        bool shareFavoritesAcrossSaves{false};
        bool enableAdvancedFilters{true};
        bool showActiveFilterNames{true};
        bool showCommandConfirmations{true};
        std::uint32_t customCommandExamplesVersion{0};
        std::string rowButtonAction{std::string(kDisabledActionID)};
        std::string doubleClickAction{std::string(kDisabledActionID)};
        std::vector<std::string> commandOrder;
        std::vector<std::string> hiddenCommands;
        std::array<CustomCommandSettings, kCustomCommandSlotCount> customCommands{};

        [[nodiscard]] ResultDensity DensityFor(UiDensityArea area) const noexcept;
        void Normalize() noexcept;
    };

    struct SettingsLoadResult
    {
        Settings settings;
        std::vector<std::string> warnings;
    };

    class SettingsRepository
    {
    public:
        explicit SettingsRepository(std::filesystem::path path);

        [[nodiscard]] SettingsLoadResult Load() const;
        [[nodiscard]] std::expected<void, std::string> Save(const Settings& settings) const;

    private:
        std::filesystem::path path_;
    };
}
