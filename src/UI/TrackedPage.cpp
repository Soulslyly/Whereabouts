#include "PCH.h"

#include "Core/SpatialPresentation.h"

#include "SKSEMenuFramework.h"
#include "Search/RuntimeIndex.h"
#include "Tracking/TrackingService.h"
#include "UI/Menu.h"

#include <format>

namespace whereabouts::ui
{
    void Menu::RenderTracked()
    {
        RefreshForIndexGeneration();
        if (RenderUninstallLockedPage()) return;
        if (ImGuiMCP::BeginChild(
                "##WhereaboutsTrackedPageScroll",
                {0.0F, 0.0F},
                ImGuiMCP::ImGuiChildFlags_None)) {
        [&] {
        ImGuiMCP::SeparatorText(TranslateText("Tracked NPCs"));
        ImGuiMCP::Spacing();
        SyncSelectedTarget();

        const auto snapshots = index_.Snapshot();
        std::vector<NpcSnapshot> tracked;
        if (snapshots) for (const auto& npc : *snapshots->catalog) {
            if (npc.tracked) tracked.push_back(npc);
        }
        const auto deathEntries = savedNpcs_.TrackedDeaths();
        const auto favoriteEntries = savedNpcs_.Favorites();
        std::vector<TrackedDeathEntry> missingBodies;
        for (const auto& entry : deathEntries) {
            if (entry.state == TrackedDeathState::BodyMissing) missingBodies.push_back(entry);
        }

        const bool trackingBusy = tracking_.Busy();
        const auto toolbarWidth = ImGuiMCP::GetContentRegionAvail().x;
        const auto* toolbarStyle = ImGuiMCP::GetStyle();
        const auto clearWidth = ImGuiMCP::CalcTextSize(TranslateText("Clear All Markers")).x +
            (toolbarStyle ? toolbarStyle->FramePadding.x * 2.0F : 8.0F);
        ImGuiMCP::BeginDisabled(tracked.empty() || trackingBusy);
        if (ImGuiMCP::Button(TranslateText("Clear All Markers"))) {
            ImGuiMCP::OpenPopup(TranslateText("Clear all tracking markers?"));
        }
        ImGuiMCP::EndDisabled();
        if (trackingBusy) {
            DelayedTooltip(
                TranslateText("Tracking is updating."),
                ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
        }
        RenderSavedListSearch(
            trackedListSearch_, trackedListPagination_, "##TrackedListSearch",
            true, toolbarWidth, clearWidth);

        CenterNextModal();
        if (ImGuiMCP::BeginPopupModal(
                TranslateText("Clear all tracking markers?"),
                nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiMCP::TextUnformatted(TranslateText(
                "Remove all active Whereabouts map markers?"));
            if (ImGuiMCP::Button(TranslateText("Clear"))) {
                const auto token = operationEpoch_.Capture();
                if (!token || !SubmitGameTask(*token, [this](OperationEpochToken token) {
                        const auto result = tracking_.ClearAll(token);
                        if (!result) logger::warn("Clear tracking failed: {}", result.error());
                        SetCommandStatus(result ? TranslateOwned("Tracking clear queued.") : result.error());
                    })) SetCommandStatus("The current game session is not ready.");
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(TranslateText("Cancel"))) ImGuiMCP::CloseCurrentPopup();
            ImGuiMCP::EndPopup();
        }

        std::vector<SavedListSearchDocument> trackedDocuments;
        trackedDocuments.reserve(tracked.size());
        for (const auto& npc : tracked) trackedDocuments.push_back(SavedListDocument(npc));
        std::vector<SavedListSearchDocument> missingDocuments;
        missingDocuments.reserve(missingBodies.size());
        for (const auto& entry : missingBodies) missingDocuments.push_back(SavedListDocument(entry));
        const auto trackedRows = FilterSavedListRows(trackedDocuments, trackedListSearch_.data());
        const auto missingRows = FilterSavedListRows(missingDocuments, trackedListSearch_.data());
        const auto filteredCount = trackedRows.size() + missingRows.size();
        const auto density = settings_.DensityFor(UiDensityArea::SavedLists);
        const bool showSecondary = ShowsSecondaryResultMetadata(density);
        const float measuredLineHeight = ImGuiMCP::GetTextLineHeightWithSpacing();
        const float rowHeight = measuredLineHeight * (showSecondary ? 2.0F : 1.0F);
        const auto pageCapacity = SavedListPageCapacity(
            density, measuredLineHeight * 10.0F, rowHeight);
        NormalizeListPagination(
            trackedListPagination_, filteredCount, pageCapacity, density);
        const auto page = VisibleSavedListPage(filteredCount, pageCapacity, trackedListPagination_);
        const auto pageEnd = page.first + page.count;

        ImGuiMCP::Spacing();
        if (tracked.empty() && missingBodies.empty()) {
            ImGuiMCP::TextUnformatted(TranslateText("No tracked NPCs."));
        } else {
            const auto capacity = TranslateFormat(
                "Active markers: {} / 100 | Missing bodies: {}",
                tracked.size(),
                missingBodies.size());
            ImGuiMCP::TextUnformatted(capacity.c_str());
            if (filteredCount == 0) {
                ImGuiMCP::TextUnformatted(TranslateText("No matching entries."));
            } else {
            const auto pushedColors = PushThemeSafeRowColors();
            const bool superCompact = IsSuperCompactResultDensity(density);
            if (ImGuiMCP::BeginTable(
                    superCompact ? "##WhereaboutsSuperCompactTrackedRows" :
                                   "##WhereaboutsTrackedRows",
                    superCompact ? 3 : 4,
                    ImGuiMCP::ImGuiTableFlags_RowBg |
                        ImGuiMCP::ImGuiTableFlags_BordersInnerH |
                        ImGuiMCP::ImGuiTableFlags_SizingStretchProp |
                        ImGuiMCP::ImGuiTableFlags_Resizable |
                        ImGuiMCP::ImGuiTableFlags_Reorderable)) {
                ImGuiMCP::TableSetupColumn(
                    TranslateText("NPC"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch,
                    superCompact ? 0.70F : 0.43F);
                if (superCompact) {
                    ImGuiMCP::TableSetupColumn(
                        TranslateText("FormID"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 110.0F);
                } else {
                    ImGuiMCP::TableSetupColumn(
                        TranslateText("Status"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch, 0.12F);
                    ImGuiMCP::TableSetupColumn(
                        TranslateText("Location"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch, 0.31F);
                }
                ImGuiMCP::TableSetupColumn(
                    TranslateText("Action"),
                    ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
                    settings_.rowButtonAction == kDisabledActionID ? 60.0F :
                        density == ResultDensity::Detailed ? 112.0F : 56.0F);
                ImGuiMCP::TableHeadersRow();

                for (std::size_t filteredIndex = 0; filteredIndex < trackedRows.size(); ++filteredIndex) {
                    if (filteredIndex < page.first || filteredIndex >= pageEnd) continue;
                    const auto& npc = tracked[trackedRows[filteredIndex]];
                    ImGuiMCP::PushID(static_cast<int>(npc.ReferenceRuntimeID()));
                    ImGuiMCP::TableNextRow(0, rowHeight);
                    RowInteraction interaction;
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
                    const auto nameHit = BeginRowInteractionCell("##trackedNpcCell", rowHeight);
                    interaction.Include(nameHit.hovered, nameHit.activated);
                    const auto nameWidth = ImGuiMCP::GetContentRegionAvail().x;
                    const bool selected = selected_ &&
                        selected_->ReferenceRuntimeID() == npc.ReferenceRuntimeID();
                    ImGuiMCP::TextUnformatted(npc.displayName.c_str());
                    if (superCompact) {
                        OverflowTooltip(npc.displayName, nameWidth);
                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                        const auto formIdHit = BeginRowInteractionCell(
                            "##trackedFormIdCell", rowHeight);
                        interaction.Include(formIdHit.hovered, formIdHit.activated);
                        ImGuiMCP::Text("%08X", npc.ReferenceRuntimeID());
                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
                        RenderQuickActionButton(npc, TargetSource::Tracked);
                        ApplyUnifiedRowBackground(interaction, selected);
                        HandleNpcRowActivation(interaction, npc, TargetSource::Tracked);
                        ImGuiMCP::PopID();
                        continue;
                    }
                    const auto identity = std::format(
                        "{} / {:08X}",
                        npc.SourcePlugin().empty() ? std::string_view{"Dynamic"} : npc.SourcePlugin(),
                        npc.ReferenceRuntimeID());
                    if (showSecondary) {
                        OverflowTooltip(npc.displayName, nameWidth);
                        const auto identityWidth = ImGuiMCP::GetContentRegionAvail().x;
                        ImGuiMCP::TextColored(
                            *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                            "%s",
                            identity.c_str());
                        OverflowTooltip(identity, identityWidth);
                    } else {
                        const auto tooltip = std::format("{}\n{}", npc.displayName, identity);
                        DelayedTooltip(tooltip.c_str());
                    }

                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                    const auto statusHit = BeginRowInteractionCell("##trackedStatusCell", rowHeight);
                    interaction.Include(statusHit.hovered, statusHit.activated);
                    const bool favorite = npc.StableReference().IsPersistable() &&
                        std::ranges::any_of(favoriteEntries, [&](const auto& entry) {
                            return entry.identity == npc.StableReference();
                        });
                    RenderDenseStatusBadges(npc, false, selected, favorite);
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
                    const auto locationHit = BeginRowInteractionCell("##trackedLocationCell", rowHeight);
                    interaction.Include(locationHit.hovered, locationHit.activated);
                    const auto location = LocalizedPrimarySpatialLabel(npc.spatial);
                    const auto locationWidth = ImGuiMCP::GetContentRegionAvail().x;
                    ImGuiMCP::TextUnformatted(location.c_str());
                    const auto worldspace = SecondaryWorldspaceLabel(npc.spatial);
                    if (showSecondary && !worldspace.empty()) {
                        OverflowTooltip(location, locationWidth);
                        const auto worldspaceWidth = ImGuiMCP::GetContentRegionAvail().x;
                        ImGuiMCP::TextColored(
                            *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                            "%s",
                            worldspace.c_str());
                        OverflowTooltip(worldspace, worldspaceWidth);
                    } else if (!showSecondary) {
                        const auto tooltip = worldspace.empty() ? location :
                            std::format("{}\n{}", location, worldspace);
                        DelayedTooltip(tooltip.c_str());
                    }
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(3));
                    RenderQuickActionButton(npc, TargetSource::Tracked);
                    ApplyUnifiedRowBackground(interaction, selected);
                    HandleNpcRowActivation(interaction, npc, TargetSource::Tracked);
                    ImGuiMCP::PopID();
                }

                for (std::size_t filteredIndex = 0; filteredIndex < missingRows.size(); ++filteredIndex) {
                    const auto combinedIndex = trackedRows.size() + filteredIndex;
                    if (combinedIndex < page.first || combinedIndex >= pageEnd) continue;
                    const auto& entry = missingBodies[missingRows[filteredIndex]];
                    const auto entryKey = entry.identity ?
                        std::format("{}:{:06X}", entry.identity->plugin, entry.identity->localID) :
                        std::format("{:08X}", entry.runtimeFormID);
                    ImGuiMCP::PushID(entryKey.c_str());
                    ImGuiMCP::TableNextRow(0, rowHeight);
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(0));
                    const auto name = entry.lastKnownName.empty() ?
                        std::string{"Unknown NPC"} : entry.lastKnownName;
                    const auto nameWidth = ImGuiMCP::GetContentRegionAvail().x;
                    ImGuiMCP::TextUnformatted(name.c_str());
                    if (superCompact) {
                        OverflowTooltip(name, nameWidth);
                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                        ImGuiMCP::Text("%08X", entry.runtimeFormID);
                        static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
                        if (ImGuiMCP::Button(TranslateText("Remove Record"))) {
                            static_cast<void>(savedNpcs_.RemoveTrackedDeath(entry));
                            ImGuiMCP::PopID();
                            ImGuiMCP::EndTable();
                            if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
                            return;
                        }
                        DelayedTooltip(TranslateText(
                            "Removes this missing-body history entry."));
                        ImGuiMCP::PopID();
                        continue;
                    }
                    const auto plugin = entry.identity && !entry.identity->plugin.empty() ?
                        entry.identity->plugin : std::string{"Dynamic"};
                    const auto identity = std::format("{} / {:08X}", plugin, entry.runtimeFormID);
                    const auto presentation = PresentSecondaryMetadata(
                        density, name, identity);
                    if (presentation.visible) {
                        OverflowTooltip(name, nameWidth);
                        const auto identityWidth = ImGuiMCP::GetContentRegionAvail().x;
                        ImGuiMCP::TextColored(
                            *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_TextDisabled),
                            "%s",
                            presentation.visible->c_str());
                        OverflowTooltip(*presentation.visible, identityWidth);
                    } else {
                        DelayedTooltip(presentation.tooltip.c_str());
                    }

                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(1));
                    RenderDenseStatusBadges({}, true);
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(2));
                    const auto location = entry.lastKnownLocation.empty() ?
                        std::string{"Last location unknown"} : entry.lastKnownLocation;
                    const auto locationWidth = ImGuiMCP::GetContentRegionAvail().x;
                    ImGuiMCP::TextUnformatted(location.c_str());
                    OverflowTooltip(location, locationWidth);
                    static_cast<void>(ImGuiMCP::TableSetColumnIndex(3));
                    if (ImGuiMCP::Button(TranslateText("Remove Record"))) {
                        static_cast<void>(savedNpcs_.RemoveTrackedDeath(entry));
                        ImGuiMCP::PopID();
                        ImGuiMCP::EndTable();
                        if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
                        return;
                    }
                    DelayedTooltip(TranslateText("Removes this missing-body history entry."));
                    ImGuiMCP::PopID();
                }
                ImGuiMCP::EndTable();
            }
            if (pushedColors > 0) ImGuiMCP::PopStyleColor(pushedColors);
            RenderSavedListPagination(
                "##TrackedPagination", filteredCount, pageCapacity, trackedListPagination_);
            }
            if (!missingBodies.empty()) {
                ImGuiMCP::TextWrapped("%s", TranslateText(
                    "M means the body is no longer available. Remove Record dismisses that history entry."));
            }
        }
        ImGuiMCP::TextWrapped("%s", TranslateText(
            "Up to 100 active markers. Missing-body records use no marker slots."));
        if (selected_) {
            ImGuiMCP::Spacing();
            RenderDetails(settings_.DensityFor(UiDensityArea::SelectedNpc));
        }
        }();
        }
        ImGuiMCP::EndChild();
        RenderRootModals();
    }
}
