#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "UI/Menu.h"
#include "WhereaboutsVersion.h"

#include <algorithm>
#include <array>
#include <cstdio>
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

        std::string ConfiguredActionLabel(const Settings& settings, std::string_view actionID)
        {
            if (actionID == kDisabledActionID) return TranslateOwned("Off");
            if (actionID == kActorFlagsGroupActionID) return TranslateOwned("Essential / Protected");
            if (actionID == kCopyIdentityActionID) return TranslateOwned("Copy ID");
            if (actionID == kCopyNpcReportActionID) return TranslateOwned("Copy NPC Report");
            if (const auto command = CommandForActionID(actionID)) {
                return TranslateOwned(CommandPolicy::Label(*command));
            }
            for (std::size_t index = 0; index < settings.customCommands.size(); ++index) {
                if (actionID == CustomActionID(index)) {
                    return settings.customCommands[index].name.empty() ?
                        TranslateFormat("Custom Command {}", index + 1) :
                        settings.customCommands[index].name;
                }
            }
            return TranslateOwned("Off");
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

        if (ImGuiMCP::Checkbox(
                TranslateText("Enable Advanced Filters"),
                &settings_.enableAdvancedFilters)) {
            if (!settings_.enableAdvancedFilters) ResetAdvancedFilters();
            saveNow = true;
        }
        DelayedTooltip(TranslateText(
            "Disabling this clears demographic, safety, spatial, worldspace, faction, and keyword filters."));
        saveNow |= ImGuiMCP::Checkbox(
            TranslateText("Show active filter names"),
            &settings_.showActiveFilterNames);
        DelayedTooltip(TranslateText(
            "Shows active filter names in section headings; hover a heading for complete values."));

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
        const std::array resultDensities{
            TranslateText("Detailed"),
            TranslateText("Compact"),
            TranslateText("Super Compact")};
        const auto densityIndex = settings_.resultDensity == ResultDensity::Compact ? 1 :
            settings_.resultDensity == ResultDensity::SuperCompact ? 2 : 0;
        ImGuiMCP::SetNextItemWidth(180.0F);
        if (ImGuiMCP::BeginCombo(TranslateText("Global UI density"), resultDensities[densityIndex])) {
            for (int index = 0; index < static_cast<int>(resultDensities.size()); ++index) {
                if (ImGuiMCP::Selectable(resultDensities[index], densityIndex == index)) {
                    settings_.resultDensity = index == 1 ? ResultDensity::Compact :
                        index == 2 ? ResultDensity::SuperCompact : ResultDensity::Detailed;
                    saveNow = true;
                }
            }
            ImGuiMCP::EndCombo();
        }
        DelayedTooltip(TranslateText(settings_.resultDensity == ResultDensity::SuperCompact ?
            "Super Compact shows only NPC name and FormID." :
            "Compact shows one line per NPC."));
        if (ImGuiMCP::CollapsingHeader(TranslateText("Density overrides"))) {
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Use Global follows the top-level density. Overrides apply immediately."));
            const std::array overrideNames{
                TranslateText("Use Global"),
                TranslateText("Detailed"),
                TranslateText("Compact"),
                TranslateText("Super Compact")};
            const auto overrideIndex = [](DensityOverride value) {
                return value == DensityOverride::Detailed ? 1 :
                    value == DensityOverride::Compact ? 2 :
                    value == DensityOverride::SuperCompact ? 3 : 0;
            };
            const auto overrideForIndex = [](int index) {
                return index == 1 ? DensityOverride::Detailed :
                    index == 2 ? DensityOverride::Compact :
                    index == 3 ? DensityOverride::SuperCompact :
                    DensityOverride::UseGlobal;
            };
            const auto renderOverride = [&](const char* label, const char* id, DensityOverride& value) {
                const int selected = overrideIndex(value);
                ImGuiMCP::SetNextItemWidth(180.0F);
                if (ImGuiMCP::BeginCombo(id, overrideNames[selected])) {
                    for (int index = 0; index < static_cast<int>(overrideNames.size()); ++index) {
                        if (ImGuiMCP::Selectable(overrideNames[index], selected == index)) {
                            value = overrideForIndex(index);
                            saveNow = true;
                        }
                    }
                    ImGuiMCP::EndCombo();
                }
                ImGuiMCP::SameLine();
                ImGuiMCP::TextUnformatted(label);
            };
            renderOverride(TranslateText("Search and select"),
                "##WhereaboutsSearchDensity", settings_.searchDensity);
            renderOverride(TranslateText("Filters and sorting"),
                "##WhereaboutsFilterDensity", settings_.filterDensity);
            renderOverride(TranslateText("Advanced filters"),
                "##WhereaboutsAdvancedFilterDensity", settings_.advancedFilterDensity);
            renderOverride(TranslateText("Results"),
                "##WhereaboutsResultsDensity", settings_.resultsDensity);
            renderOverride(TranslateText("Selected NPC and commands"),
                "##WhereaboutsSelectedNpcDensity", settings_.selectedNpcDensity);
            renderOverride(TranslateText("Recent, Favorites, and Tracked"),
                "##WhereaboutsSavedListsDensity", settings_.savedListsDensity);
            renderOverride(TranslateText("NPC Inspector"),
                "##WhereaboutsInspectorDensity", settings_.inspectorDensity);
        }
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
        bool shareFavorites = settings_.shareFavoritesAcrossSaves;
        if (ImGuiMCP::Checkbox(
                TranslateText("Share Favorites between saves"), &shareFavorites)) {
            const auto updated = favorites_.SetSharingEnabled(shareFavorites);
            if (updated) {
                settings_.shareFavoritesAcrossSaves = shareFavorites;
                savedEntriesStatus_ = shareFavorites ?
                    TranslateOwned("Favorites from this save and the shared list were merged.") :
                    TranslateOwned("Favorites sharing is off. The shared list was preserved.");
                saveNow = true;
            } else {
                savedEntriesStatus_ = TranslateFormat(
                    "Favorites sharing could not be changed: {}", updated.error());
            }
        }
        DelayedTooltip(TranslateText(
            "When enabled, stable Favorites are shared across saves and playthroughs for this Skyrim installation."));
        if (!savedEntriesStatus_.empty()) {
            ImGuiMCP::TextWrapped("%s", savedEntriesStatus_.c_str());
        }

        ImGuiMCP::Spacing();
        ImGuiMCP::SeparatorText(TranslateText("Commands"));
        if (ImGuiMCP::Checkbox(
                TranslateText("Show command confirmations"),
                &settings_.showCommandConfirmations)) {
            if (!settings_.showCommandConfirmations) {
                pendingCommand_.reset();
                pendingCommandRuntimeID_ = 0;
                openCommandConfirmation_ = false;
                openTrackingWarning_ = false;
                openLocationTravelConfirmation_ = false;
            }
            saveNow = true;
        }
        DelayedTooltip(TranslateText(
            "When off, built-in NPC commands, built-in quick actions, tracking, and location travel run without confirmation prompts. Custom commands always run immediately after validation."));

        const auto renderActionAssignment = [&](
            const char* label,
            const char* id,
            std::string& assignment) {
            ImGuiMCP::SetNextItemWidth(240.0F);
            const auto preview = ConfiguredActionLabel(settings_, assignment);
            if (!ImGuiMCP::BeginCombo(id, preview.c_str())) return;
            if (ImGuiMCP::Selectable(
                    TranslateText("Off"), assignment == kDisabledActionID)) {
                assignment = kDisabledActionID;
                saveNow = true;
            }
            for (const auto& choice : kBuiltInCommandActions) {
                if (ImGuiMCP::Selectable(
                        TranslateText(CommandPolicy::Label(choice.command)),
                        assignment == choice.id)) {
                    assignment = std::string(choice.id);
                    saveNow = true;
                }
            }
            if (ImGuiMCP::Selectable(
                    TranslateText("Copy NPC Report"),
                    assignment == kCopyNpcReportActionID)) {
                assignment = kCopyNpcReportActionID;
                saveNow = true;
            }
            for (std::size_t index = 0; index < settings_.customCommands.size(); ++index) {
                const auto& custom = settings_.customCommands[index];
                if (!custom.enabled) continue;
                const auto actionID = CustomActionID(index);
                if (ImGuiMCP::Selectable(
                        custom.name.c_str(), assignment == actionID)) {
                    assignment = actionID;
                    saveNow = true;
                }
            }
            ImGuiMCP::EndCombo();
            DelayedTooltip(label);
        };
        renderActionAssignment(
            TranslateText("Action used by the small NPC-row button."),
            "##WhereaboutsRowButtonAction",
            settings_.rowButtonAction);
        ImGuiMCP::SameLine();
        ImGuiMCP::TextUnformatted(TranslateText("Row button action"));
        renderActionAssignment(
            TranslateText("Action used when an NPC result row is double-clicked."),
            "##WhereaboutsDoubleClickAction",
            settings_.doubleClickAction);
        ImGuiMCP::SameLine();
        ImGuiMCP::TextUnformatted(TranslateText("Double-click action"));

        if (ImGuiMCP::CollapsingHeader(TranslateText("Command layout"))) {
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Show, hide, and reorder command actions. Hidden actions remain available for Quick Action and double-click assignments."));
            auto actions = OrderedAvailableCommandActions(settings_);
            std::optional<std::pair<std::size_t, std::size_t>> move;
            if (ImGuiMCP::BeginTable(
                    "##WhereaboutsCommandLayout",
                    2,
                    ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
                ImGuiMCP::TableSetupColumn(
                    TranslateText("Visible"),
                    ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
                ImGuiMCP::TableSetupColumn(
                    "##WhereaboutsCommandReorder",
                    ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
                    68.0F);
                for (std::size_t index = 0; index < actions.size(); ++index) {
                    const auto& actionID = actions[index];
                    ImGuiMCP::PushID(actionID.c_str());
                    ImGuiMCP::TableNextRow();
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
                    bool visible = std::ranges::find(
                        settings_.hiddenCommands, actionID) == settings_.hiddenCommands.end();
                    if (ImGuiMCP::Checkbox("##CommandVisible", &visible)) {
                        if (visible) {
                            std::erase(settings_.hiddenCommands, actionID);
                        } else if (std::ranges::find(
                                settings_.hiddenCommands, actionID) ==
                                settings_.hiddenCommands.end()) {
                            settings_.hiddenCommands.push_back(actionID);
                        }
                        saveNow = true;
                    }
                    ImGuiMCP::SameLine();
                    const auto actionLabel = ConfiguredActionLabel(settings_, actionID);
                    const auto labelWidth = ImGuiMCP::GetContentRegionAvail().x;
                    ImGuiMCP::TextUnformatted(actionLabel.c_str());
                    OverflowTooltip(actionLabel, labelWidth);

                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                    ImGuiMCP::BeginDisabled(index == 0);
                    if (ImGuiMCP::SmallButton("^##MoveUp")) {
                        move = std::pair{index, index - 1};
                    }
                    ImGuiMCP::EndDisabled();
                    DelayedTooltip(
                        TranslateText("Move up"),
                        ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
                    ImGuiMCP::SameLine();
                    ImGuiMCP::BeginDisabled(index + 1 >= actions.size());
                    if (ImGuiMCP::SmallButton("v##MoveDown")) {
                        move = std::pair{index, index + 1};
                    }
                    ImGuiMCP::EndDisabled();
                    DelayedTooltip(
                        TranslateText("Move down"),
                        ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
                    ImGuiMCP::PopID();
                }
                ImGuiMCP::EndTable();
            }
            if (move) {
                std::swap(actions[move->first], actions[move->second]);
                settings_.commandOrder = std::move(actions);
                saveNow = true;
            }
        }

        if (!customCommandBuffersLoaded_) {
            for (std::size_t index = 0; index < settings_.customCommands.size(); ++index) {
                std::snprintf(
                    customCommandNameBuffers_[index].data(),
                    customCommandNameBuffers_[index].size(),
                    "%s", settings_.customCommands[index].name.c_str());
                std::snprintf(
                    customCommandTextBuffers_[index].data(),
                    customCommandTextBuffers_[index].size(),
                    "%s", settings_.customCommands[index].command.c_str());
            }
            customCommandBuffersLoaded_ = true;
        }
        if (ImGuiMCP::CollapsingHeader(TranslateText("Custom commands"))) {
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Use {refid} and {baseid} for resolved eight-digit FormIDs. Commands are dispatched to exactly one selected NPC."));
            for (std::size_t index = 0; index < settings_.customCommands.size(); ++index) {
                ImGuiMCP::PushID(static_cast<int>(index));
                auto& custom = settings_.customCommands[index];
                if (ImGuiMCP::Checkbox(TranslateText("Enabled"), &custom.enabled)) {
                    saveNow = true;
                }
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(180.0F);
                if (ImGuiMCP::InputText(
                        "##CustomCommandName",
                        customCommandNameBuffers_[index].data(),
                        customCommandNameBuffers_[index].size())) {
                    custom.name = customCommandNameBuffers_[index].data();
                    saveNow = true;
                }
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(-1.0F);
                if (ImGuiMCP::InputText(
                        "##CustomCommandText",
                        customCommandTextBuffers_[index].data(),
                        customCommandTextBuffers_[index].size())) {
                    custom.command = customCommandTextBuffers_[index].data();
                    saveNow = true;
                }
                const auto validation = ValidateCustomCommand(custom.name, custom.command);
                if (!validation && (!custom.name.empty() || !custom.command.empty())) {
                    ImGuiMCP::TextDisabled("%s", validation.error().c_str());
                }
                ImGuiMCP::PopID();
            }
        }

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
                customCommandBuffersLoaded_ = false;
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
                "Requires the matching SKSE and Address Library, Skyrim VR ESL Support on VR, and either SKSE Menu Framework or ApocryphaRealm Menu Framework."));
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "Thanks to k0mp1ex for Where Are You, an awesome mod and the inspiration for Whereabouts."));
            ImGuiMCP::TextWrapped("%s", TranslateText(
                "SKSE Menu Framework API by QTR-Modding, used under LGPL-2.1."));
        }
        RenderRootModals();
    }
}
