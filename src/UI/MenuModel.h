#pragma once

#include "Core/TextFold.h"
#include "Core/RuntimeIndexSnapshot.h"

#include "Commands/CommandPolicy.h"
#include "Core/TrackingCompletion.h"
#include "Persistence/Settings.h"
#include "Search/SearchText.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace whereabouts::ui
{
    inline constexpr std::array<const char*, 6> kPageNames{
        "Search", "Locations", "Tracked NPCs", "Favorites", "Recent", "Settings"};
    inline constexpr const char* kFollowerFilterLabel{"Follower"};

    enum class SearchContent
    {
        NpcsOnly,
        NpcsAndLocations,
        LocationsOnly
    };

    enum class ResultSectionOrder
    {
        NpcsFirst,
        LocationsFirst
    };

    [[nodiscard]] constexpr bool IncludesNpcs(SearchContent content) noexcept
    {
        return content != SearchContent::LocationsOnly;
    }

    [[nodiscard]] constexpr bool IncludesLocations(SearchContent content) noexcept
    {
        return content != SearchContent::NpcsOnly;
    }

    [[nodiscard]] constexpr bool LocationsAppearFirst(
        SearchContent content,
        ResultSectionOrder order) noexcept
    {
        return content == SearchContent::NpcsAndLocations &&
            order == ResultSectionOrder::LocationsFirst;
    }

    [[nodiscard]] constexpr int SearchToolbarColumns(float availableWidth) noexcept
    {
        if (availableWidth >= 1050.0F) return 5;
        if (availableWidth >= 700.0F) return 3;
        if (availableWidth >= 440.0F) return 2;
        return 1;
    }

    [[nodiscard]] constexpr int ResponsiveControlColumns(
        float availableWidth,
        float minimumColumnWidth,
        float itemSpacing,
        int maximumColumns) noexcept
    {
        constexpr float infinity = std::numeric_limits<float>::infinity();
        if (!(availableWidth > 0.0F) || !(availableWidth < infinity) ||
            !(minimumColumnWidth > 0.0F) || !(minimumColumnWidth < infinity) ||
            maximumColumns < 2) {
            return 1;
        }
        const float spacing = itemSpacing > 0.0F ? itemSpacing : 0.0F;
        for (int columns = maximumColumns; columns > 1; --columns) {
            const float required = minimumColumnWidth * static_cast<float>(columns) +
                spacing * static_cast<float>(columns - 1);
            if (availableWidth >= required) return columns;
        }
        return 1;
    }

    struct CombinedResultAllocation
    {
        std::size_t primary{0};
        std::size_t secondary{0};

        [[nodiscard]] bool operator==(const CombinedResultAllocation&) const noexcept = default;
    };

    [[nodiscard]] constexpr CombinedResultAllocation FitCombinedResults(
        std::size_t limit,
        std::size_t primaryAvailable,
        std::size_t secondaryAvailable) noexcept
    {
        if (limit == 0) return {};

        const auto secondaryReserve = secondaryAvailable == 0 ? 0 :
            (std::min)(secondaryAvailable, (std::max<std::size_t>)(1, limit / 2));
        const auto primary = (std::min)(primaryAvailable, limit - secondaryReserve);
        const auto secondary = (std::min)(secondaryAvailable, limit - primary);
        return {primary, secondary};
    }

    [[nodiscard]] constexpr bool ShouldRunLocationSearch(
        bool mainSearch,
        SearchContent content) noexcept
    {
        return !mainSearch || IncludesLocations(content);
    }

    enum class IndexUiState
    {
        Preparing,
        Ready,
        FailedEmpty,
        FailedWithCatalog
    };

    [[nodiscard]] constexpr IndexUiState ClassifyIndexUi(
        IndexReadiness readiness,
        IndexFailure,
        bool hasCatalog) noexcept
    {
        if (readiness == IndexReadiness::Failed) {
            return hasCatalog ? IndexUiState::FailedWithCatalog : IndexUiState::FailedEmpty;
        }
        if (readiness == IndexReadiness::Ready) return IndexUiState::Ready;
        return IndexUiState::Preparing;
    }

    [[nodiscard]] constexpr std::string_view IndexFailureText(IndexFailure failure) noexcept
    {
        switch (failure) {
        case IndexFailure::TaskInterfaceUnavailable:
            return "Search index could not start. Use Refresh Search Index in Settings.";
        case IndexFailure::FormsUnavailable:
            return "Game forms are not ready. Use Refresh Search Index in Settings.";
        case IndexFailure::BuildFailed:
            return "Search index refresh failed. Use Refresh Search Index in Settings.";
        case IndexFailure::None:
            return "Search index is unavailable. Use Refresh Search Index in Settings.";
        }
        return "Search index is unavailable. Use Refresh Search Index in Settings.";
    }

    enum class ManualRefreshState
    {
        Idle,
        Waiting,
        Complete,
        Failed
    };

    [[nodiscard]] constexpr ManualRefreshState ObserveManualRefresh(
        ManualRefreshState state,
        std::uint64_t baselineSession,
        std::uint64_t baselineRevision,
        std::uint64_t currentSession,
        std::uint64_t currentRevision,
        IndexReadiness readiness,
        IndexFailure failure) noexcept
    {
        if (state != ManualRefreshState::Waiting) return state;
        const bool changed = currentSession != baselineSession || currentRevision > baselineRevision;
        if (!changed) return state;
        if (readiness == IndexReadiness::Failed || failure != IndexFailure::None) {
            return ManualRefreshState::Failed;
        }
        return readiness == IndexReadiness::Ready ? ManualRefreshState::Complete : state;
    }

    struct GenericFilterState
    {
        bool includeGeneric{false};
        bool automaticContext{false};
    };

    [[nodiscard]] constexpr GenericFilterState TransitionGenericFilter(
        GenericFilterState current,
        bool automaticContext) noexcept
    {
        if (current.automaticContext == automaticContext) return current;
        return GenericFilterState{automaticContext, automaticContext};
    }

    [[nodiscard]] inline std::vector<std::string> StatusTagLabels(
        const NpcSnapshot& npc,
        bool favorite = false)
    {
        std::vector<std::string> labels;
        labels.reserve(6);
        labels.emplace_back(npc.alive ? "Alive" : "Dead");
        labels.emplace_back(npc.enabled ? "Enabled" : "Disabled");
        labels.emplace_back(npc.loaded ? "Loaded" : "Unloaded");
        if (npc.teammate) labels.emplace_back("Follower");
        if (npc.potentialFollower) labels.emplace_back("Potential Follower");
        if (npc.tracked) labels.emplace_back("Tracked");
        if (favorite) labels.emplace_back("Favorite");
        if (!npc.IsUniqueBase()) labels.emplace_back("Generic");
        return labels;
    }

    enum class CopyIdentityKind
    {
        FormID,
        EditorID,
        Stable
    };

    [[nodiscard]] constexpr std::string_view CopyIdentityLabel(
        CopyIdentityKind kind) noexcept
    {
        switch (kind) {
        case CopyIdentityKind::FormID: return "FormID";
        case CopyIdentityKind::EditorID: return "EditorID";
        case CopyIdentityKind::Stable: return "Stable";
        }
        return "FormID";
    }

    [[nodiscard]] inline std::optional<std::string> CopyIdentityValue(
        const NpcSnapshot& npc,
        CopyIdentityKind kind)
    {
        switch (kind) {
        case CopyIdentityKind::FormID:
            return npc.ReferenceRuntimeID() == 0 ? std::nullopt :
                std::optional{std::format("{:08X}", npc.ReferenceRuntimeID())};
        case CopyIdentityKind::Stable:
            if (!npc.StableReference().IsPersistable()) return std::nullopt;
            return std::format(
                "{}:{:06X}", npc.StableReference().plugin, npc.StableReference().localID);
        case CopyIdentityKind::EditorID:
            return npc.baseEditorID.empty() ? std::nullopt :
                std::optional{npc.baseEditorID};
        }
        return std::nullopt;
    }

    struct CopyIdentitySelection
    {
        std::string value;
        CopyIdentityKind requested{CopyIdentityKind::FormID};
        CopyIdentityKind copied{CopyIdentityKind::FormID};
        bool usedFallback{false};
    };

    [[nodiscard]] inline CopyIdentitySelection ResolveCopyIdentity(
        const NpcSnapshot& npc,
        CopyIdentityKind requested)
    {
        if (const auto requestedValue = CopyIdentityValue(npc, requested)) {
            return {*requestedValue, requested, requested, false};
        }
        if (const auto formID = CopyIdentityValue(npc, CopyIdentityKind::FormID)) {
            return {*formID, requested, CopyIdentityKind::FormID, true};
        }
        return {{}, requested, requested, false};
    }

    [[nodiscard]] inline std::string CopyIdentityConfirmation(
        const CopyIdentitySelection& selection)
    {
        if (selection.value.empty()) return "No ID is available for this NPC.";
        if (selection.usedFallback) {
            return std::format(
                "No {} - copied {} {}.",
                CopyIdentityLabel(selection.requested),
                CopyIdentityLabel(selection.copied),
                selection.value);
        }
        return std::format(
            "Copied {} {}.", CopyIdentityLabel(selection.copied), selection.value);
    }

    enum class LocationCopyKind
    {
        FormID,
        EditorID,
        Stable,
        CocCommand
    };

    [[nodiscard]] constexpr std::string_view LocationCopyLabel(
        LocationCopyKind kind) noexcept
    {
        switch (kind) {
        case LocationCopyKind::FormID: return "FormID";
        case LocationCopyKind::EditorID: return "EditorID";
        case LocationCopyKind::Stable: return "Stable";
        case LocationCopyKind::CocCommand: return "COC command";
        }
        return "FormID";
    }

    [[nodiscard]] inline std::optional<std::string> LocationCopyValue(
        const LocationSnapshot& location,
        LocationCopyKind kind)
    {
        switch (kind) {
        case LocationCopyKind::FormID:
            return location.runtimeFormID == 0 ? std::nullopt :
                std::optional{std::format("{:08X}", location.runtimeFormID)};
        case LocationCopyKind::EditorID:
            return location.editorID.empty() ? std::nullopt :
                std::optional{location.editorID};
        case LocationCopyKind::Stable:
            if (!location.identity.IsPersistable()) return std::nullopt;
            return std::format(
                "{}:{:06X}", location.identity.plugin, location.identity.localID);
        case LocationCopyKind::CocCommand:
            return location.editorID.empty() ? std::nullopt :
                std::optional{std::format("coc {}", location.editorID)};
        }
        return std::nullopt;
    }

    struct LocationCopySelection
    {
        std::string value;
        LocationCopyKind requested{LocationCopyKind::FormID};
        LocationCopyKind copied{LocationCopyKind::FormID};
        bool usedFallback{false};
    };

    [[nodiscard]] inline LocationCopySelection ResolveLocationCopy(
        const LocationSnapshot& location,
        LocationCopyKind requested)
    {
        if (const auto requestedValue = LocationCopyValue(location, requested)) {
            return {*requestedValue, requested, requested, false};
        }
        if (const auto formID = LocationCopyValue(location, LocationCopyKind::FormID)) {
            return {*formID, requested, LocationCopyKind::FormID, true};
        }
        return {{}, requested, requested, false};
    }

    [[nodiscard]] inline std::string LocationCopyConfirmation(
        const LocationCopySelection& selection)
    {
        if (selection.value.empty()) return "No ID is available for this location.";
        if (selection.usedFallback) {
            return std::format(
                "No {} - copied {} {}.",
                LocationCopyLabel(selection.requested),
                LocationCopyLabel(selection.copied),
                selection.value);
        }
        return std::format(
            "Copied {} {}.", LocationCopyLabel(selection.copied), selection.value);
    }

    enum class SearchRun
    {
        Preview,
        Submitted
    };

    [[nodiscard]] inline std::string TriStateFilterPreview(
        std::string_view label,
        std::string_view state)
    {
        return std::format("{}: {}", label, state);
    }

    enum class SearchMissRecovery
    {
        None,
        ClearFilters,
        RefreshIndex
    };

    [[nodiscard]] constexpr SearchMissRecovery ClassifySearchMissRecovery(
        SearchRun run,
        bool hasError,
        std::size_t textMatchTotal,
        std::size_t npcMatchTotal,
        std::size_t locationMatchTotal,
        std::size_t activeFilterCount) noexcept
    {
        if (run != SearchRun::Submitted || hasError ||
            npcMatchTotal + locationMatchTotal != 0) {
            return SearchMissRecovery::None;
        }
        if (textMatchTotal != 0 && activeFilterCount != 0) {
            return SearchMissRecovery::ClearFilters;
        }
        return SearchMissRecovery::RefreshIndex;
    }

    [[nodiscard]] constexpr bool ShouldOfferTypoSuggestions(
        SearchRun run,
        SearchTextKind kind,
        std::size_t resultCount,
        bool hasActiveFilters) noexcept
    {
        return run == SearchRun::Submitted &&
            kind == SearchTextKind::Name &&
            resultCount == 0 &&
            !hasActiveFilters;
    }

    struct RowInteraction
    {
        bool hovered{false};
        bool activated{false};

        constexpr void Include(bool cellHovered, bool cellActivated) noexcept
        {
            hovered = hovered || cellHovered;
            activated = activated || cellActivated;
        }
    };

    [[nodiscard]] constexpr bool ShowsSecondaryResultMetadata(
        ResultDensity density) noexcept
    {
        return density == ResultDensity::Detailed;
    }

    struct SecondaryMetadataPresentation
    {
        std::optional<std::string> visible;
        std::string tooltip;
    };

    [[nodiscard]] inline SecondaryMetadataPresentation PresentSecondaryMetadata(
        ResultDensity density,
        std::string_view primary,
        std::string_view secondary)
    {
        if (secondary.empty()) return {};
        if (ShowsSecondaryResultMetadata(density)) {
            return {std::string{secondary}, {}};
        }
        if (primary.empty()) return {std::nullopt, std::string{secondary}};
        return {
            std::nullopt,
            std::format("{}\n{}", primary, secondary)};
    }

    [[nodiscard]] constexpr float ClampSearchPaneRatio(float ratio) noexcept
    {
        return std::clamp(ratio, 0.3F, 0.7F);
    }

    [[nodiscard]] constexpr bool UseWideLocationControls(float availableWidth) noexcept
    {
        return availableWidth >= 900.0F;
    }

    [[nodiscard]] constexpr float ComputeStackedPaneHeight(
        float availableHeight,
        float ratio) noexcept
    {
        const float usableHeight = (std::max)(1.0F, availableHeight);
        float paneHeight = usableHeight * ClampSearchPaneRatio(ratio);
        if (usableHeight >= 360.0F) {
            paneHeight = std::clamp(paneHeight, 180.0F, usableHeight - 180.0F);
        }
        return std::clamp(paneHeight, 1.0F, usableHeight);
    }

    enum class SearchColumnId : std::uint32_t
    {
        Npc = 1,
        Status = 2,
        Location = 3
    };

    [[nodiscard]] constexpr std::optional<std::pair<SortKey, bool>> SearchHeaderSort(
        std::uint32_t columnId,
        bool ascending) noexcept
    {
        if (columnId == static_cast<std::uint32_t>(SearchColumnId::Npc)) {
            return std::pair{SortKey::Name, ascending};
        }
        if (columnId == static_cast<std::uint32_t>(SearchColumnId::Location)) {
            return std::pair{SortKey::Location, ascending};
        }
        if (columnId == static_cast<std::uint32_t>(SearchColumnId::Status)) {
            return std::pair{SortKey::Status, ascending};
        }
        return std::nullopt;
    }

    [[nodiscard]] constexpr bool SortUsesDirection(SortKey key) noexcept
    {
        return key != SortKey::Random;
    }

    [[nodiscard]] constexpr std::uint64_t NextRandomSeed(std::uint64_t seed) noexcept
    {
        return seed + 0x9E3779B97F4A7C15ULL;
    }

    [[nodiscard]] constexpr SearchRun SearchRunForRefresh(SearchRun current) noexcept
    {
        return current;
    }

    class SearchRefreshState
    {
    public:
        void Select(SearchRun run) noexcept
        {
            current_ = run;
        }

        [[nodiscard]] SearchRun Current() const noexcept
        {
            return current_;
        }

        void Request() noexcept
        {
            requested_.store(true, std::memory_order_release);
        }

        [[nodiscard]] std::optional<SearchRun> Consume() noexcept
        {
            if (!requested_.exchange(false, std::memory_order_acq_rel)) return std::nullopt;
            return SearchRunForRefresh(current_);
        }

    private:
        SearchRun current_{SearchRun::Preview};
        std::atomic_bool requested_{false};
    };

    enum class ResultFooter
    {
        None,
        ExpandPreview,
        RefineSearch
    };

    [[nodiscard]] constexpr std::size_t RemainingResultCount(
        std::size_t shown,
        std::size_t total) noexcept
    {
        return total > shown ? total - shown : 0;
    }

    [[nodiscard]] inline std::string ResultSummary(std::size_t shown, std::size_t total)
    {
        return std::format("{} shown / {} matches", shown, total);
    }

    [[nodiscard]] constexpr ResultFooter SelectResultFooter(
        SearchRun run,
        std::size_t shown,
        std::size_t total,
        bool moreRowsBelow) noexcept
    {
        if (moreRowsBelow) return ResultFooter::None;
        if (shown >= total) return ResultFooter::None;
        return run == SearchRun::Preview ? ResultFooter::ExpandPreview :
                                           ResultFooter::RefineSearch;
    }

    enum class DenseStatusBadge
    {
        Favorite,
        Tracked,
        Follower,
        Generic,
        Alive,
        Dead,
        Enabled,
        Disabled,
        Loaded,
        Unloaded,
        Missing
    };

    [[nodiscard]] constexpr std::optional<DenseStatusBadge> DenseStatusBadgeForTag(
        std::string_view tag) noexcept
    {
        if (tag == "Favorite") return DenseStatusBadge::Favorite;
        if (tag == "Tracked") return DenseStatusBadge::Tracked;
        if (tag == "Follower") return DenseStatusBadge::Follower;
        if (tag == "Generic") return DenseStatusBadge::Generic;
        if (tag == "Alive") return DenseStatusBadge::Alive;
        if (tag == "Dead") return DenseStatusBadge::Dead;
        if (tag == "Enabled") return DenseStatusBadge::Enabled;
        if (tag == "Disabled") return DenseStatusBadge::Disabled;
        if (tag == "Loaded") return DenseStatusBadge::Loaded;
        if (tag == "Unloaded") return DenseStatusBadge::Unloaded;
        return std::nullopt;
    }

    struct SemanticColor
    {
        float red;
        float green;
        float blue;

        [[nodiscard]] bool operator==(const SemanticColor&) const noexcept = default;
    };

    [[nodiscard]] constexpr SemanticColor CompositeThemeBackground(
        SemanticColor child,
        float childAlpha,
        SemanticColor window) noexcept
    {
        const auto alpha = std::clamp(childAlpha, 0.0F, 1.0F);
        return {
            child.red * alpha + window.red * (1.0F - alpha),
            child.green * alpha + window.green * (1.0F - alpha),
            child.blue * alpha + window.blue * (1.0F - alpha)};
    }

    [[nodiscard]] constexpr SemanticColor CompositeTableRowBackground(
        SemanticColor base,
        SemanticColor row,
        float rowAlpha) noexcept
    {
        return CompositeThemeBackground(row, rowAlpha, base);
    }

    [[nodiscard]] constexpr bool IsAlternateTableBodyRow(
        int currentRow,
        int headerRows = 1) noexcept
    {
        return currentRow >= headerRows && ((currentRow - headerRows) & 1) != 0;
    }

    [[nodiscard]] constexpr bool IsDarkThemeBackground(SemanticColor background) noexcept
    {
        return 0.2126F * background.red + 0.7152F * background.green +
                   0.0722F * background.blue <
               0.50F;
    }

    [[nodiscard]] constexpr SemanticColor DenseStatusBadgeColor(
        DenseStatusBadge badge,
        bool darkBackground) noexcept
    {
        if (darkBackground) {
            switch (badge) {
            case DenseStatusBadge::Favorite: return {1.00F, 0.74F, 0.18F};
            case DenseStatusBadge::Tracked: return {0.25F, 0.85F, 1.00F};
            case DenseStatusBadge::Follower: return {1.00F, 0.45F, 0.85F};
            case DenseStatusBadge::Generic: return {1.00F, 0.80F, 0.20F};
            case DenseStatusBadge::Alive: return {0.35F, 0.95F, 0.45F};
            case DenseStatusBadge::Dead: return {1.00F, 0.35F, 0.35F};
            case DenseStatusBadge::Enabled: return {0.25F, 1.00F, 0.72F};
            case DenseStatusBadge::Disabled: return {1.00F, 0.55F, 0.20F};
            case DenseStatusBadge::Loaded: return {0.45F, 0.65F, 1.00F};
            case DenseStatusBadge::Unloaded: return {0.65F, 0.70F, 0.75F};
            case DenseStatusBadge::Missing: return {0.75F, 0.45F, 1.00F};
            }
        } else {
            switch (badge) {
            case DenseStatusBadge::Favorite: return {0.62F, 0.38F, 0.00F};
            case DenseStatusBadge::Tracked: return {0.00F, 0.40F, 0.62F};
            case DenseStatusBadge::Follower: return {0.70F, 0.08F, 0.50F};
            case DenseStatusBadge::Generic: return {0.55F, 0.36F, 0.00F};
            case DenseStatusBadge::Alive: return {0.00F, 0.46F, 0.12F};
            case DenseStatusBadge::Dead: return {0.72F, 0.00F, 0.00F};
            case DenseStatusBadge::Enabled: return {0.00F, 0.43F, 0.30F};
            case DenseStatusBadge::Disabled: return {0.68F, 0.27F, 0.00F};
            case DenseStatusBadge::Loaded: return {0.14F, 0.30F, 0.72F};
            case DenseStatusBadge::Unloaded: return {0.34F, 0.39F, 0.44F};
            case DenseStatusBadge::Missing: return {0.46F, 0.18F, 0.70F};
            }
        }
        return darkBackground ? SemanticColor{1.0F, 1.0F, 1.0F} :
                                SemanticColor{0.0F, 0.0F, 0.0F};
    }

    [[nodiscard]] inline double SrgbChannelLuminance(float channel) noexcept
    {
        const double value = std::clamp(static_cast<double>(channel), 0.0, 1.0);
        if (value <= 0.04045) return value / 12.92;
        return std::pow((value + 0.055) / 1.055, 2.4);
    }

    [[nodiscard]] inline double RelativeLuminance(SemanticColor color) noexcept
    {
        return 0.2126 * SrgbChannelLuminance(color.red) +
            0.7152 * SrgbChannelLuminance(color.green) +
            0.0722 * SrgbChannelLuminance(color.blue);
    }

    [[nodiscard]] inline double ContrastRatio(
        SemanticColor foreground,
        SemanticColor background) noexcept
    {
        const auto foregroundLuminance = RelativeLuminance(foreground);
        const auto backgroundLuminance = RelativeLuminance(background);
        const auto lighter = foregroundLuminance >= backgroundLuminance ?
            foregroundLuminance : backgroundLuminance;
        const auto darker = foregroundLuminance < backgroundLuminance ?
            foregroundLuminance : backgroundLuminance;
        return (lighter + 0.05) / (darker + 0.05);
    }

    [[nodiscard]] constexpr SemanticColor MixSemanticColor(
        SemanticColor from,
        SemanticColor to,
        float amount) noexcept
    {
        const auto clamped = std::clamp(amount, 0.0F, 1.0F);
        return {
            from.red + (to.red - from.red) * clamped,
            from.green + (to.green - from.green) * clamped,
            from.blue + (to.blue - from.blue) * clamped};
    }

    [[nodiscard]] constexpr SemanticColor RowInteractionColor(
        SemanticColor background,
        SemanticColor text,
        bool selected) noexcept
    {
        return MixSemanticColor(background, text, selected ? 0.28F : 0.13F);
    }

    [[nodiscard]] inline SemanticColor EnsureTextContrast(
        SemanticColor foreground,
        SemanticColor background,
        double minimumRatio = 4.5) noexcept
    {
        if (ContrastRatio(foreground, background) >= minimumRatio) return foreground;

        constexpr SemanticColor black{0.0F, 0.0F, 0.0F};
        constexpr SemanticColor white{1.0F, 1.0F, 1.0F};
        const auto target = ContrastRatio(black, background) >= ContrastRatio(white, background) ?
            black : white;

        float low = 0.0F;
        float high = 1.0F;
        for (int iteration = 0; iteration < 20; ++iteration) {
            const auto middle = (low + high) * 0.5F;
            if (ContrastRatio(MixSemanticColor(foreground, target, middle), background) >=
                minimumRatio) {
                high = middle;
            } else {
                low = middle;
            }
        }
        return MixSemanticColor(foreground, target, high);
    }

    [[nodiscard]] inline SemanticColor ContrastAdjustedStatusBadgeColor(
        DenseStatusBadge badge,
        SemanticColor background) noexcept
    {
        const auto base = DenseStatusBadgeColor(badge, IsDarkThemeBackground(background));
        return EnsureTextContrast(base, background);
    }

    [[nodiscard]] constexpr char DenseStatusBadgeLetter(DenseStatusBadge badge) noexcept
    {
        switch (badge) {
        case DenseStatusBadge::Favorite: return 'V';
        case DenseStatusBadge::Tracked: return 'T';
        case DenseStatusBadge::Follower: return 'F';
        case DenseStatusBadge::Generic: return 'G';
        case DenseStatusBadge::Alive: return 'A';
        case DenseStatusBadge::Dead: return 'D';
        case DenseStatusBadge::Enabled: return 'E';
        case DenseStatusBadge::Disabled: return 'X';
        case DenseStatusBadge::Loaded: return 'L';
        case DenseStatusBadge::Unloaded: return 'U';
        case DenseStatusBadge::Missing: return 'M';
        }
        return '?';
    }

    [[nodiscard]] constexpr std::string_view DenseStatusBadgeTooltip(
        DenseStatusBadge badge) noexcept
    {
        switch (badge) {
        case DenseStatusBadge::Favorite: return "Favorite";
        case DenseStatusBadge::Tracked: return "Tracked";
        case DenseStatusBadge::Follower: return "Follower";
        case DenseStatusBadge::Generic: return "Generic NPC";
        case DenseStatusBadge::Alive: return "Alive";
        case DenseStatusBadge::Dead: return "Dead; body present";
        case DenseStatusBadge::Enabled: return "Enabled";
        case DenseStatusBadge::Disabled: return "Disabled";
        case DenseStatusBadge::Loaded: return "Loaded";
        case DenseStatusBadge::Unloaded: return "Unloaded";
        case DenseStatusBadge::Missing: return "Body no longer available";
        }
        return "Unknown status";
    }

    [[nodiscard]] inline std::vector<DenseStatusBadge> DenseStatusBadges(
        const NpcSnapshot& npc,
        bool favorite = false)
    {
        std::vector<DenseStatusBadge> badges;
        badges.reserve(9);
        if (favorite) badges.push_back(DenseStatusBadge::Favorite);
        if (npc.tracked) badges.push_back(DenseStatusBadge::Tracked);
        if (npc.teammate) badges.push_back(DenseStatusBadge::Follower);
        if (!npc.IsUniqueBase()) badges.push_back(DenseStatusBadge::Generic);
        badges.push_back(npc.alive ? DenseStatusBadge::Alive : DenseStatusBadge::Dead);
        badges.push_back(npc.enabled ? DenseStatusBadge::Enabled : DenseStatusBadge::Disabled);
        badges.push_back(npc.loaded ? DenseStatusBadge::Loaded : DenseStatusBadge::Unloaded);
        return badges;
    }

    [[nodiscard]] constexpr bool NeedsOverflowTooltip(
        float textWidth,
        float availableWidth) noexcept
    {
        return availableWidth > 0.0F && textWidth > availableWidth;
    }

    [[nodiscard]] constexpr bool ShouldDeriveAlternateRowColors(
        float rowAlpha,
        float alternateAlpha,
        float rgbDistance) noexcept
    {
        const auto alphaDistance = rowAlpha > alternateAlpha ?
            rowAlpha - alternateAlpha : alternateAlpha - rowAlpha;
        const bool visible = rowAlpha > 0.025F || alternateAlpha > 0.025F;
        const bool distinct = rgbDistance > 0.025F || alphaDistance > 0.025F;
        return !visible || !distinct;
    }

    enum class ControllerEdit
    {
        Character,
        Space,
        Backspace,
        Clear
    };

    using ControllerRows = std::array<std::string_view, 4>;

    [[nodiscard]] constexpr ControllerRows KeyboardRows(
        ControllerKeyboardLayout layout) noexcept
    {
        if (layout == ControllerKeyboardLayout::Qwerty) {
            return {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM", "0123456789"};
        }
        return {"ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ", "0123456789"};
    }

    [[nodiscard]] inline bool ApplyControllerEdit(
        std::span<char> buffer,
        ControllerEdit edit,
        char character = '\0') noexcept
    {
        if (buffer.size() < 2) return false;
        const auto end = std::ranges::find(buffer, '\0');
        if (end == buffer.end()) return false;
        const auto length = static_cast<std::size_t>(end - buffer.begin());

        if (edit == ControllerEdit::Clear) {
            if (length == 0) return false;
            buffer.front() = '\0';
            return true;
        }
        if (edit == ControllerEdit::Backspace) {
            if (length == 0) return false;
            auto start = length - 1;
            while (start > 0 && (static_cast<unsigned char>(buffer[start]) & 0xC0U) == 0x80U) --start;
            buffer[start] = '\0';
            return true;
        }

        const char value = edit == ControllerEdit::Space ? ' ' : character;
        if (value == '\0' || length + 1 >= buffer.size()) return false;
        buffer[length] = value;
        buffer[length + 1] = '\0';
        return true;
    }

    [[nodiscard]] constexpr std::size_t ResultLimit(
        SearchRun run,
        const Settings& settings) noexcept
    {
        return run == SearchRun::Preview ? settings.liveResultLimit : settings.fullResultLimit;
    }

    [[nodiscard]] constexpr bool ResultsUnlimited(
        SearchRun,
        const Settings& settings) noexcept
    {
        return settings.showAllResults;
    }

    enum class SavedRowAction
    {
        Select,
        Remove
    };

    enum class SavedEntryAvailability
    {
        Available,
        PluginMissing,
        ReferenceUnavailable
    };

    [[nodiscard]] constexpr SavedEntryAvailability ClassifySavedEntryAvailability(
        bool snapshotAvailable,
        bool pluginLoaded) noexcept
    {
        if (snapshotAvailable) return SavedEntryAvailability::Available;
        return pluginLoaded ? SavedEntryAvailability::ReferenceUnavailable :
                              SavedEntryAvailability::PluginMissing;
    }

    enum class PaneLayout
    {
        SideBySide,
        Stacked
    };

    [[nodiscard]] constexpr PaneLayout ChoosePaneLayout(float availableWidth) noexcept
    {
        return availableWidth >= 850.0F ? PaneLayout::SideBySide : PaneLayout::Stacked;
    }

    [[nodiscard]] constexpr bool NeedsTrackingWarning(
        CommandKind command,
        bool currentlyTracked,
        bool acknowledged) noexcept
    {
        return command == CommandKind::Track && !currentlyTracked && !acknowledged;
    }

    [[nodiscard]] constexpr bool NeedsDifferentWorldspaceTrackNotice(
        bool succeeded,
        TrackingOperation operation,
        MovementBoundary boundary) noexcept
    {
        return succeeded && operation == TrackingOperation::Track &&
               boundary == MovementBoundary::DifferentWorldspace;
    }

    [[nodiscard]] inline std::vector<std::string> FilterPluginNames(
        const std::vector<std::string>& names,
        std::string_view typed,
        std::size_t limit)
    {
        const auto normalizedTyped = FoldTextForSearch(typed);
        if (normalizedTyped.empty() || limit == 0) return {};

        struct Candidate
        {
            std::string value;
            std::string normalized;
            int rank;
        };

        std::vector<Candidate> candidates;
        for (const auto& name : names) {
            const auto normalized = FoldTextForSearch(name);
            const auto position = normalized.find(normalizedTyped);
            if (position == std::string::npos) continue;
            if (std::ranges::any_of(candidates, [&](const Candidate& candidate) {
                    return candidate.normalized == normalized;
                })) {
                continue;
            }
            const auto rank = normalized == normalizedTyped ? 0 : (position == 0 ? 1 : 2);
            candidates.push_back(Candidate{name, normalized, rank});
        }

        std::ranges::sort(candidates, [](const Candidate& left, const Candidate& right) {
            if (left.rank != right.rank) return left.rank < right.rank;
            return left.normalized < right.normalized;
        });
        if (candidates.size() > limit) candidates.resize(limit);

        std::vector<std::string> result;
        result.reserve(candidates.size());
        for (auto& candidate : candidates) result.push_back(std::move(candidate.value));
        return result;
    }
}
