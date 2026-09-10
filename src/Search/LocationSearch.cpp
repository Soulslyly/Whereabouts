#include "Search/LocationSearch.h"

#include "Core/TextFold.h"
#include "Search/SearchText.h"

#include <algorithm>

namespace whereabouts
{
    namespace
    {
        constexpr int kNoMatch = 100;

        int TextRank(std::string_view value, std::string_view query) noexcept
        {
            if (query.empty()) return 0;
            if (value == query) return 0;
            if (value.starts_with(query)) return 1;
            const auto word = std::string{" "} + std::string{query};
            if (value.find(word) != std::string_view::npos) return 2;
            return value.find(query) != std::string_view::npos ? 3 : kNoMatch;
        }

        int MatchRank(const LocationSnapshot& row, const ParsedSearchText& parsed) noexcept
        {
            switch (parsed.kind) {
            case SearchTextKind::NumericFormID:
                if (parsed.formID == row.runtimeFormID) return 0;
                return parsed.formID <= 0x00FFFFFF && parsed.formID == row.identity.localID ?
                    1 : kNoMatch;
            case SearchTextKind::StableIdentity:
                return parsed.identity && parsed.identity->localID == row.identity.localID &&
                    SearchTextEqualsNoexcept(parsed.identity->plugin, row.identity.plugin) ? 0 : kNoMatch;
            case SearchTextKind::Name:
                break;
            }

            const auto folded = FoldTextForSearch(parsed.name);
            const auto nameRank = TextRank(row.searchName, folded);
            if (nameRank != kNoMatch) return nameRank;
            const auto editorRank = TextRank(row.searchEditorID, folded);
            if (editorRank != kNoMatch) return 10 + editorRank;
            const auto contextRank = (std::min)(
                TextRank(row.searchContext, folded),
                TextRank(row.searchWorldspace, folded));
            return contextRank == kNoMatch ? kNoMatch : 20 + contextRank;
        }

        bool ContainsFolded(std::string_view value, const std::optional<std::string>& filter)
        {
            return !filter || value.find(FoldTextForSearch(*filter)) != std::string_view::npos;
        }
    }

    LocationSearchMatches SearchLocations(
        std::span<const LocationSnapshot> source,
        const LocationSearchQuery& query)
    {
        const auto parsed = ParseSearchText(query.text);
        if (!parsed) return {};

        struct RankedRow
        {
            const LocationSnapshot* row;
            int rank;
        };
        std::vector<RankedRow> matches;
        matches.reserve(source.size());
        for (const auto& row : source) {
            if (!query.selectedPlugins.empty()) {
                const auto selected = std::ranges::any_of(
                    query.selectedPlugins,
                    [&](const std::string& plugin) {
                        return SearchTextEqualsNoexcept(plugin, row.identity.plugin);
                    });
                if (!selected) continue;
            } else if (!ContainsFolded(row.searchPlugin, query.plugin)) {
                continue;
            }
            if (query.context &&
                !ContainsFolded(row.searchContext, query.context) &&
                !ContainsFolded(row.searchWorldspace, query.context) &&
                !ContainsFolded(row.searchName, query.context) &&
                !ContainsFolded(row.searchEditorID, query.context)) continue;
            const auto rank = MatchRank(row, *parsed);
            if (rank != kNoMatch) matches.push_back({&row, rank});
        }

        const auto key = [&](const LocationSnapshot& row) -> std::string_view {
            switch (query.sort) {
            case LocationSortKey::Plugin: return row.searchPlugin;
            case LocationSortKey::Worldspace: return row.searchWorldspace;
            case LocationSortKey::Name: return row.searchName;
            }
            return row.searchName;
        };
        std::ranges::sort(matches, [&](const RankedRow& left, const RankedRow& right) {
            if (left.rank != right.rank) return left.rank < right.rank;
            const auto leftKey = key(*left.row);
            const auto rightKey = key(*right.row);
            if (leftKey != rightKey) return query.ascending ? leftKey < rightKey : leftKey > rightKey;
            if (left.row->searchName != right.row->searchName) {
                return left.row->searchName < right.row->searchName;
            }
            return left.row->runtimeFormID < right.row->runtimeFormID;
        });

        LocationSearchMatches result;
        result.total = matches.size();
        constexpr std::size_t kMaximumResults = 1000;
        const auto normalizedLimit = std::clamp<std::size_t>(query.limit, 1, kMaximumResults);
        const auto count = query.unlimitedResults ? matches.size() :
            (std::min)(normalizedLimit, matches.size());
        result.visible.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            result.visible.push_back(*matches[index].row);
        }
        return result;
    }
}
