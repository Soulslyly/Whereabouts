#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "UI/Menu.h"
#include "WhereaboutsVersion.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace whereabouts::ui
{
    namespace
    {
        constexpr std::array kLanguageChoices{
            TranslationLanguage::FollowSkyrim,
            TranslationLanguage::English,
            TranslationLanguage::Chinese,
            TranslationLanguage::French,
            TranslationLanguage::German,
            TranslationLanguage::Italian,
            TranslationLanguage::Japanese,
            TranslationLanguage::Polish,
            TranslationLanguage::Russian,
            TranslationLanguage::Spanish};

        const char* LanguageLabel(TranslationLanguage language) noexcept
        {
            switch (language) {
            case TranslationLanguage::FollowSkyrim: return "Follow Skyrim (default)";
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
            return "Follow Skyrim (default)";
        }

        const char* LanguageLabel(std::string_view code) noexcept
        {
            if (code == "CHINESE") return "Chinese";
            if (code == "FRENCH") return "French";
            if (code == "GERMAN") return "German";
            if (code == "ITALIAN") return "Italian";
            if (code == "JAPANESE") return "Japanese";
            if (code == "POLISH") return "Polish";
            if (code == "RUSSIAN") return "Russian";
            if (code == "SPANISH") return "Spanish";
            return "English";
        }
    }

    void Menu::RenderSettings()
    {
        RefreshForIndexGeneration();
        if (IsUninstallLocked(uninstallPhase_)) {
            RenderUninstallLockedSettings();
            RenderRootModals();
            return;
        }
        bool saveNow = false;
        ImGuiMCP::SeparatorText(TranslateText("Settings"));
        ImGuiMCP::Spacing();
        ImGuiMCP::SeparatorText(TranslateText("Target selection"));
        saveNow |= ImGuiMCP::Checkbox(
            TranslateText("Auto-select console target"),
            &settings_.autoSelectConsoleTarget);
        DelayedTooltip(TranslateText("Uses the console-selected NPC when Whereabouts opens."));
        saveNow |= ImGuiMCP::Checkbox(
            TranslateText("Auto-select crosshair target"),
            &settings_.autoSelectCrosshairTarget);
        DelayedTooltip(TranslateText("Uses the NPC under the crosshair when Whereabouts opens."));

        ImGuiMCP::Spacing();
        ImGuiMCP::SeparatorText(TranslateText("Display and search"));
        const auto selectedLanguage = std::ranges::find(
            kLanguageChoices, settings_.translationLanguage);
        const auto languageIndex = selectedLanguage == kLanguageChoices.end() ?
            0 : static_cast<int>(std::distance(kLanguageChoices.begin(), selectedLanguage));
        ImGuiMCP::SetNextItemWidth(220.0F);
        if (ImGuiMCP::BeginCombo(
                TranslateText("Language"),
                TranslateText(LanguageLabel(kLanguageChoices[languageIndex])))) {
            for (int index = 0; index < static_cast<int>(kLanguageChoices.size()); ++index) {
                if (ImGuiMCP::Selectable(
                        TranslateText(LanguageLabel(kLanguageChoices[index])),
                        languageIndex == index)) {
                    settings_.translationLanguage = kLanguageChoices[index];
                    saveNow = true;
                }
            }
            ImGuiMCP::EndCombo();
        }
        const auto loadedLanguage = LoadedTranslationLanguage();
        ImGuiMCP::TextDisabled(
            "%s",
            TranslateFormat(
                "Loaded language: {}",
                TranslateOwned(LanguageLabel(loadedLanguage))).c_str());
        if (ResolveConfiguredTranslationLanguage(settings_.translationLanguage) != loadedLanguage) {
            ImGuiMCP::TextWrapped(
                "%s", TranslateText("Restart Skyrim to apply this language."));
        }

        const std::array keyboardLayouts{TranslateText("Alphabetical"), "QWERTY"};
        const auto keyboardIndex = settings_.controllerKeyboardLayout ==
            ControllerKeyboardLayout::Qwerty ? 1 : 0;
        ImGuiMCP::SetNextItemWidth(180.0F);
        if (ImGuiMCP::BeginCombo(TranslateText("Virtual Keyboard layout"), keyboardLayouts[keyboardIndex])) {
            for (int index = 0; index < static_cast<int>(keyboardLayouts.size()); ++index) {
                if (ImGuiMCP::Selectable(keyboardLayouts[index], keyboardIndex == index)) {
                    settings_.controllerKeyboardLayout = index == 1 ?
                        ControllerKeyboardLayout::Qwerty :
                        ControllerKeyboardLayout::Alphabetical;
                    saveNow = true;
                }
            }
            ImGuiMCP::EndCombo();
        }
        const std::array distanceUnits{TranslateText("Meters"), TranslateText("Feet"), TranslateText("Game units")};
        const auto distanceIndex = settings_.distanceUnit == DistanceUnit::Feet ? 1 :
            settings_.distanceUnit == DistanceUnit::GameUnits ? 2 : 0;
        ImGuiMCP::SetNextItemWidth(180.0F);
        if (ImGuiMCP::BeginCombo(TranslateText("Distance unit"), distanceUnits[distanceIndex])) {
            for (int index = 0; index < static_cast<int>(distanceUnits.size()); ++index) {
                if (ImGuiMCP::Selectable(distanceUnits[index], distanceIndex == index)) {
                    settings_.distanceUnit = index == 1 ? DistanceUnit::Feet :
                        index == 2 ? DistanceUnit::GameUnits : DistanceUnit::Meters;
                    saveNow = true;
                }
            }
            ImGuiMCP::EndCombo();
        }
        const std::array resultDensities{TranslateText("Detailed"), TranslateText("Compact")};
        const auto densityIndex = settings_.resultDensity == ResultDensity::Compact ? 1 : 0;
        ImGuiMCP::SetNextItemWidth(180.0F);
        if (ImGuiMCP::BeginCombo(TranslateText("Result detail"), resultDensities[densityIndex])) {
            for (int index = 0; index < static_cast<int>(resultDensities.size()); ++index) {
                if (ImGuiMCP::Selectable(resultDensities[index], densityIndex == index)) {
                    settings_.resultDensity = index == 1 ?
                        ResultDensity::Compact : ResultDensity::Detailed;
                    saveNow = true;
                }
            }
            ImGuiMCP::EndCombo();
        }
        DelayedTooltip(TranslateText("Compact shows one line per NPC."));
        ImGuiMCP::BeginDisabled(settings_.showAllResults);
        int liveResultLimit = static_cast<int>(settings_.liveResultLimit);
        if (ImGuiMCP::SliderInt(
                TranslateText("Live results"), &liveResultLimit, 10, 100, "%d")) {
            settings_.liveResultLimit = static_cast<std::uint32_t>(liveResultLimit);
        }
        saveNow |= ImGuiMCP::IsItemDeactivatedAfterEdit();
        DelayedTooltip(TranslateText("Maximum rows shown while typing."));
        int fullResultLimit = static_cast<int>(settings_.fullResultLimit);
        if (ImGuiMCP::SliderInt(
                TranslateText("Full results"), &fullResultLimit, 50, 1000, "%d")) {
            settings_.fullResultLimit = static_cast<std::uint32_t>(fullResultLimit);
        }
        saveNow |= ImGuiMCP::IsItemDeactivatedAfterEdit();
        DelayedTooltip(TranslateText("Maximum rows shown after Search or Enter."));
        ImGuiMCP::EndDisabled();
        saveNow |= ImGuiMCP::Checkbox(
            TranslateText("Show all results"), &settings_.showAllResults);
        DelayedTooltip(TranslateText(
            "Removes both result limits; the scroll area still keeps the menu responsive."));

        ImGuiMCP::Spacing();
        ImGuiMCP::SeparatorText(TranslateText("Tracking"));
        saveNow |= ImGuiMCP::Checkbox(TranslateText("Notify when a tracked NPC dies"), &settings_.notifyTrackedDeath);

        ImGuiMCP::Spacing();
        ImGuiMCP::SeparatorText(TranslateText("Maintenance"));
        if (ImGuiMCP::Button(TranslateText("Refresh Search Index"))) {
            QueueIndexRefresh();
        }
        DelayedTooltip(TranslateText("Rebuilds the searchable NPC list."));
        ImGuiMCP::SameLine();
        ImGuiMCP::TextDisabled("%s", TranslateText("Rebuild search data"));
        if (!indexRefreshStatus_.empty()) {
            ImGuiMCP::TextWrapped("%s", indexRefreshStatus_.c_str());
        }
        ImGuiMCP::Spacing();
        ImGuiMCP::BeginDisabled(
            uninstallPhase_ == UninstallPhase::ClearingQuest ||
            uninstallPhase_ == UninstallPhase::Complete);
        if (ImGuiMCP::Button(TranslateText("Prepare for Uninstall"))) RequestPrepareForUninstall();
        ImGuiMCP::EndDisabled();
        DelayedTooltip(
            TranslateText(uninstallPhase_ == UninstallPhase::ClearingQuest ?
                "Cleanup is already running." :
                uninstallPhase_ == UninstallPhase::Complete ?
                    "Cleanup complete. Save, quit Skyrim, then remove the mod." :
                    "Clears markers and Whereabouts save lists. Then save, quit Skyrim, and remove the mod."),
            uninstallPhase_ == UninstallPhase::ClearingQuest || uninstallPhase_ == UninstallPhase::Complete ?
                ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled : 0);
        if (!uninstallStatus_.empty()) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextWrapped("%s", uninstallStatus_.c_str());
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::CollapsingHeader(TranslateText("Advanced"))) {
            ImGuiMCP::Spacing();
            saveNow |= ImGuiMCP::Checkbox(TranslateText("Live search"), &settings_.liveSearch);
            static_cast<void>(ImGuiMCP::SliderFloat(
                TranslateText("Teleport separation"),
                &settings_.teleportRange,
                0.0F,
                1000.0F,
                "%.0f"));
            saveNow |= ImGuiMCP::IsItemDeactivatedAfterEdit();
            saveNow |= ImGuiMCP::Checkbox(TranslateText("Confirm disable"), &settings_.confirmDisable);
            saveNow |= ImGuiMCP::Checkbox(TranslateText("Confirm teleport"), &settings_.confirmTeleport);
            saveNow |= ImGuiMCP::Checkbox(
                TranslateText("Keep menu open after inline command"),
                &settings_.keepOpenAfterInlineCommand);

            saveNow |= ImGuiMCP::Checkbox(TranslateText("Debug logging"), &settings_.debugLogging);
        }

        if (saveNow) SaveSettings();

        ImGuiMCP::Spacing();
        ImGuiMCP::SeparatorText(TranslateText("Reset"));
        if (ImGuiMCP::Button(TranslateText("Restore Default Settings"))) {
            ImGuiMCP::OpenPopup(TranslateText("Restore Whereabouts defaults?"));
        }
        if (!settingsStatus_.empty()) ImGuiMCP::TextWrapped("%s", settingsStatus_.c_str());
        CenterNextModal();
        if (ImGuiMCP::BeginPopupModal(
                TranslateText("Restore Whereabouts defaults?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::TextUnformatted(TranslateText(
                "Restore every global Whereabouts setting to its default value?"));
            if (ImGuiMCP::Button(TranslateText("Restore"))) {
                settings_ = Settings{};
                ResetFiltersToDefaults();
                SaveSettings();
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(TranslateText("Cancel"))) ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::CollapsingHeader(TranslateText("About Whereabouts"))) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextUnformatted(TranslateText("Whereabouts - Search, Locate, and Track NPCs"));
            ImGuiMCP::TextUnformatted(whereabouts::version::kAbout.data());
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Requires SKSE, Address Library for SKSE Plugins, and SKSE Menu Framework."));
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Thanks to k0mp1ex for Where Are You, an awesome mod and the inspiration for Whereabouts."));
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "SKSE Menu Framework API by QTR-Modding, used under LGPL-2.1."));
        }
        RenderRootModals();
    }
}
