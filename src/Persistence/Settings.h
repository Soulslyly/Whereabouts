#pragma once

#include "Core/DistanceFormat.h"
#include "Search/SearchEngine.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace whereabouts
{
    enum class ControllerKeyboardLayout
    {
        Alphabetical,
        Qwerty
    };

    enum class ResultDensity
    {
        Detailed,
        Compact
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
        TranslationLanguage translationLanguage{TranslationLanguage::FollowSkyrim};
        std::uint32_t liveResultLimit{50};
        std::uint32_t fullResultLimit{500};
        bool showAllResults{false};

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
