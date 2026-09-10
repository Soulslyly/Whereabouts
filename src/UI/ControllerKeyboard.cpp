#include "PCH.h"

#include "SKSEMenuFramework.h"
#include "UI/Menu.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace whereabouts::ui
{
    void Menu::RenderControllerKeyboard()
    {
        CenterNextModal();
        if (!ImGuiMCP::BeginPopupModal(
                TranslateText("Whereabouts Virtual Keyboard"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            return;
        }

        ImGuiMCP::TextUnformatted(searchText_.data());
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();
        const auto applyEdit = [&](ControllerEdit edit, char character = '\0') {
            if (ApplyControllerEdit(std::span<char>{searchText_}, edit, character) &&
                settings_.liveSearch) {
                RunSearch(SearchRun::Preview);
            }
        };
        for (const auto row : KeyboardRows(settings_.controllerKeyboardLayout)) {
            for (std::size_t index = 0; index < row.size(); ++index) {
                const char label[2]{row[index], '\0'};
                if (ImGuiMCP::Button(label, {34.0F, 0.0F})) {
                    applyEdit(ControllerEdit::Character, row[index]);
                }
                if (index + 1 < row.size()) ImGuiMCP::SameLine();
            }
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::Button(TranslateText("Backspace"))) {
            applyEdit(ControllerEdit::Backspace);
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Clear"))) applyEdit(ControllerEdit::Clear);
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Space"))) {
            applyEdit(ControllerEdit::Space);
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::Button(TranslateText("Apply"))) {
            RunSearch(SearchRun::Submitted, true);
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(TranslateText("Cancel"))) {
            searchText_.fill('\0');
            const auto maximum = searchText_.size() - 1;
            const auto length = controllerBackup_.size() < maximum ? controllerBackup_.size() : maximum;
            std::copy_n(controllerBackup_.data(), length, searchText_.data());
            results_ = controllerResultsBackup_;
            searchLocationResults_ = controllerLocationResultsBackup_;
            searchSuggestions_ = controllerSuggestionsBackup_;
            resultTotal_ = controllerResultTotalBackup_;
            resultTextMatchTotal_ = controllerResultTextMatchTotalBackup_;
            searchLocationResultTotal_ = controllerLocationResultTotalBackup_;
            searchError_ = controllerSearchErrorBackup_;
            searchRefreshState_.Select(controllerSearchRunBackup_);
            ImGuiMCP::CloseCurrentPopup();
        }
        ImGuiMCP::EndPopup();
    }
}
