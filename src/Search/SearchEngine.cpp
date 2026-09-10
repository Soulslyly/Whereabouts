#include "Search/SearchEngine.h"
#include "Search/SearchText.h"
#include "Core/TextFold.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace whereabouts
{
    namespace
    {
        constexpr std::size_t kMinimumResults = 1;
        constexpr std::size_t kMaximumResults = 1000;

        [[nodiscard]] bool MatchesIdentity(
            const FormIdentity& identity,
            const StableIdentityQuery& query)
        {
            return identity.localID == query.localID &&
                   SearchTextEquals(identity.plugin, query.plugin);
        }

        [[nodiscard]] bool MatchesNumericFormID(
            const NpcSnapshot& npc,
            std::uint32_t formID)
        {
            if (npc.ReferenceRuntimeID() == formID || npc.BaseRuntimeID() == formID) return true;
            if (formID > 0x00FFFFFF) return false;
            return (npc.StableReference().IsPersistable() &&
                    npc.StableReference().localID == formID) ||
                   (npc.StableBase().IsPersistable() && npc.StableBase().localID == formID);
        }

        [[nodiscard]] bool MatchesExactRuntimeFormID(
            const NpcSnapshot& npc,
            std::uint32_t formID)
        {
            return npc.ReferenceRuntimeID() == formID || npc.BaseRuntimeID() == formID;
        }

        enum class TextMatchRank
        {
            Exact,
            Prefix,
            WordPrefix,
            Substring,
            NoMatch
        };

        [[nodiscard]] bool IsAsciiWordCharacter(char value) noexcept
        {
            const auto byte = static_cast<unsigned char>(value);
            return std::isalnum(byte) != 0 || value == '_';
        }

        [[nodiscard]] bool IsGuaranteedAsciiSeparator(char value) noexcept
        {
            const auto byte = static_cast<unsigned char>(value);
            return byte < 0x80U && !IsAsciiWordCharacter(value);
        }

        [[nodiscard]] TextMatchRank RankTextMatch(
            std::string_view normalizedValue,
            std::string_view normalizedFragment)
        {
            if (normalizedFragment.empty()) return TextMatchRank::Exact;
            if (normalizedValue == normalizedFragment) return TextMatchRank::Exact;
            if (normalizedValue.starts_with(normalizedFragment)) return TextMatchRank::Prefix;

            auto position = normalizedValue.find(normalizedFragment);
            while (position != std::string::npos) {
                if (position == 0 || IsGuaranteedAsciiSeparator(normalizedValue[position - 1])) {
                    return TextMatchRank::WordPrefix;
                }
                position = normalizedValue.find(normalizedFragment, position + 1);
            }
            return normalizedValue.contains(normalizedFragment) ?
                TextMatchRank::Substring : TextMatchRank::NoMatch;
        }

        [[nodiscard]] int RankNpcNameMatch(
            const NpcSnapshot& npc,
            std::string_view normalizedFragment)
        {
            const auto nameRank = RankTextMatch(npc.searchKeys.name, normalizedFragment);
            if (nameRank != TextMatchRank::NoMatch) return static_cast<int>(nameRank);
            const auto editorRank = RankTextMatch(npc.searchKeys.editorIDs, normalizedFragment);
            return editorRank == TextMatchRank::NoMatch ?
                static_cast<int>(TextMatchRank::NoMatch) + 10 :
                static_cast<int>(editorRank) + 10;
        }

        [[nodiscard]] std::optional<std::vector<std::uint32_t>> DecodeUtf8(
            std::string_view value)
        {
            std::vector<std::uint32_t> result;
            result.reserve(value.size());
            for (std::size_t index = 0; index < value.size();) {
                const auto lead = static_cast<unsigned char>(value[index]);
                std::uint32_t codePoint = 0;
                std::size_t width = 0;
                std::uint32_t minimum = 0;
                if (lead <= 0x7FU) {
                    codePoint = lead;
                    width = 1;
                } else if ((lead & 0xE0U) == 0xC0U) {
                    codePoint = lead & 0x1FU;
                    width = 2;
                    minimum = 0x80U;
                } else if ((lead & 0xF0U) == 0xE0U) {
                    codePoint = lead & 0x0FU;
                    width = 3;
                    minimum = 0x800U;
                } else if ((lead & 0xF8U) == 0xF0U) {
                    codePoint = lead & 0x07U;
                    width = 4;
                    minimum = 0x10000U;
                } else {
                    return std::nullopt;
                }
                if (index + width > value.size()) return std::nullopt;
                for (std::size_t offset = 1; offset < width; ++offset) {
                    const auto continuation =
                        static_cast<unsigned char>(value[index + offset]);
                    if ((continuation & 0xC0U) != 0x80U) return std::nullopt;
                    codePoint = (codePoint << 6U) | (continuation & 0x3FU);
                }
                if ((width > 1 && codePoint < minimum) ||
                    (codePoint >= 0xD800U && codePoint <= 0xDFFFU) ||
                    codePoint > 0x10FFFFU) {
                    return std::nullopt;
                }
                result.push_back(codePoint);
                index += width;
            }
            return result;
        }

        [[nodiscard]] std::size_t DamerauLevenshtein(
            const std::vector<std::uint32_t>& left,
            const std::vector<std::uint32_t>& right)
        {
            std::vector<std::size_t> previousPrevious(right.size() + 1);
            std::vector<std::size_t> previous(right.size() + 1);
            std::vector<std::size_t> current(right.size() + 1);
            for (std::size_t column = 0; column <= right.size(); ++column) {
                previous[column] = column;
            }
            for (std::size_t row = 1; row <= left.size(); ++row) {
                current[0] = row;
                for (std::size_t column = 1; column <= right.size(); ++column) {
                    const auto cost = left[row - 1] == right[column - 1] ? 0U : 1U;
                    auto distance = (std::min)({
                        previous[column] + 1,
                        current[column - 1] + 1,
                        previous[column - 1] + cost});
                    if (row > 1 && column > 1 &&
                        left[row - 1] == right[column - 2] &&
                        left[row - 2] == right[column - 1]) {
                        distance = (std::min)(
                            distance,
                            previousPrevious[column - 2] + 1);
                    }
                    current[column] = distance;
                }
                previousPrevious.swap(previous);
                previous.swap(current);
            }
            return previous.back();
        }

        [[nodiscard]] bool IsFavorite(const NpcSnapshot& npc, const SearchQuery& query)
        {
            return npc.StableReference().IsPersistable() &&
                std::ranges::any_of(query.favoriteIdentities, [&](const auto& identity) {
                    return identity == npc.StableReference();
                });
        }

        [[nodiscard]] bool MatchesFilters(
            const NpcSnapshot& npc,
            const SearchQuery& query,
            std::string_view pluginFilter,
            std::string_view locationFilter)
        {
            const auto& filters = query.filters;
            if (!filters.includeGeneric && !npc.IsUniqueBase()) {
                return false;
            }
            if (filters.genericOnly && npc.IsUniqueBase()) {
                return false;
            }
            if (!filters.selectedPlugins.empty()) {
                const auto selected = std::ranges::any_of(
                    filters.selectedPlugins,
                    [&](const std::string& plugin) {
                        return SearchTextEqualsNoexcept(plugin, npc.SourcePlugin());
                    });
                if (!selected) return false;
            } else if (filters.plugin && !npc.searchKeys.plugin.contains(pluginFilter)) {
                return false;
            }
            if (filters.minimumLevel && npc.level < *filters.minimumLevel) {
                return false;
            }
            if (filters.maximumLevel && npc.level > *filters.maximumLevel) {
                return false;
            }
            if (filters.alive && npc.alive != *filters.alive) {
                return false;
            }
            if (filters.enabled && npc.enabled != *filters.enabled) {
                return false;
            }
            if (filters.teammate && npc.teammate != *filters.teammate) {
                return false;
            }
            if (filters.potentialFollower &&
                npc.potentialFollower != *filters.potentialFollower) {
                return false;
            }
            if (filters.loaded && npc.loaded != *filters.loaded) {
                return false;
            }
            if (filters.location && !npc.searchKeys.location.contains(locationFilter)) {
                return false;
            }
            if (filters.favoritesOnly && !IsFavorite(npc, query)) {
                return false;
            }
            if (filters.sameLocationOnly &&
                !npc.spatial.sameCell && !npc.spatial.sameLocation) {
                return false;
            }
            return !filters.trackedOnly || npc.tracked;
        }

        template <class Value>
        [[nodiscard]] int CompareValue(const Value& left, const Value& right)
        {
            return left < right ? -1 : (right < left ? 1 : 0);
        }

        [[nodiscard]] std::array<int, 7> StatusSortKey(
            const NpcSnapshot& npc,
            const SearchQuery& query)
        {
            return {
                IsFavorite(npc, query) ? 0 : 1,
                npc.tracked ? 0 : 1,
                npc.teammate ? 0 : 1,
                npc.IsUniqueBase() ? 0 : 1,
                npc.alive ? 0 : 1,
                npc.enabled ? 0 : 1,
                npc.loaded ? 0 : 1};
        }

        [[nodiscard]] constexpr std::uint64_t MixRandomKey(std::uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ULL;
            value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
            value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
            return value ^ (value >> 31U);
        }

        [[nodiscard]] int ComparePrimary(
            const NpcSnapshot& left,
            const NpcSnapshot& right,
            const SearchQuery& query)
        {
            switch (query.sort) {
            case SortKey::Name:
                return left.searchKeys.name.compare(right.searchKeys.name);
            case SortKey::Plugin:
                return left.searchKeys.plugin.compare(right.searchKeys.plugin);
            case SortKey::Level:
                return CompareValue(left.level, right.level);
            case SortKey::Location:
                return left.searchKeys.location.compare(right.searchKeys.location);
            case SortKey::Loaded:
                return CompareValue(left.loaded, right.loaded);
            case SortKey::Distance:
                if (left.spatial.distance.has_value() != right.spatial.distance.has_value()) {
                    return left.spatial.distance ? -1 : 1;
                }
                if (!left.spatial.distance) {
                    return 0;
                }
                return CompareValue(*left.spatial.distance, *right.spatial.distance);
            case SortKey::Status:
                return CompareValue(StatusSortKey(left, query), StatusSortKey(right, query));
            case SortKey::Random:
                return CompareValue(
                    MixRandomKey(query.randomSeed ^ left.ReferenceRuntimeID()),
                    MixRandomKey(query.randomSeed ^ right.ReferenceRuntimeID()));
            }
            return 0;
        }
    }

    SearchResult Search(RuntimeIndexView sourceView, const SearchQuery& query)
    {
        if (!sourceView || !sourceView->catalog) {
            return SearchError{"Search index is unavailable"};
        }
        const auto& source = *sourceView->catalog;
        const auto foldedPluginFilter = query.filters.plugin ?
            FoldTextForSearch(*query.filters.plugin) : std::string{};
        const auto foldedLocationFilter = query.filters.location ?
            FoldTextForSearch(*query.filters.location) : std::string{};
        ParsedSearchText parsed{.kind = SearchTextKind::Name, .name = query.text};
        const auto parsedText = ParseSearchText(query.text);
        if (!parsedText) return SearchError{parsedText.error().message};
        parsed = *parsedText;
        const auto foldedText = FoldTextForSearch(parsed.name);
        if (parsed.kind == SearchTextKind::NumericFormID) {
            const bool runtimeExists = std::ranges::any_of(source, [&parsed](const NpcSnapshot& npc) {
                return MatchesNumericFormID(npc, parsed.formID);
            });
            if (!runtimeExists) {
                parsed.kind = SearchTextKind::Name;
            }
        }

        std::vector<std::size_t> matches;
        matches.reserve(source.size());
        std::size_t textMatchTotal = 0;
        for (std::size_t index = 0; index < source.size(); ++index) {
            const auto& npc = source[index];
            bool textMatches = parsed.name.empty();
            if (!textMatches) {
                switch (parsed.kind) {
                case SearchTextKind::Name:
                    textMatches = RankNpcNameMatch(npc, foldedText) <
                        static_cast<int>(TextMatchRank::NoMatch) + 10;
                    break;
                case SearchTextKind::NumericFormID:
                    textMatches = MatchesNumericFormID(npc, parsed.formID);
                    break;
                case SearchTextKind::StableIdentity:
                    textMatches = parsed.identity &&
                        (MatchesIdentity(npc.StableReference(), *parsed.identity) ||
                         MatchesIdentity(npc.StableBase(), *parsed.identity));
                    break;
                }
            }
            if (textMatches) ++textMatchTotal;
            if (textMatches && MatchesFilters(
                    npc, query, foldedPluginFilter, foldedLocationFilter)) {
                matches.push_back(index);
            }
        }

        std::stable_sort(
            matches.begin(),
            matches.end(),
            [&query, &parsed, &source, &foldedText](std::size_t leftIndex, std::size_t rightIndex) {
                const auto& left = source[leftIndex];
                const auto& right = source[rightIndex];
                if (parsed.kind == SearchTextKind::NumericFormID) {
                    const bool leftExact = MatchesExactRuntimeFormID(left, parsed.formID);
                    const bool rightExact = MatchesExactRuntimeFormID(right, parsed.formID);
                    if (leftExact != rightExact) return leftExact;
                }
                if (parsed.kind == SearchTextKind::Name && !parsed.name.empty()) {
                    const auto leftRank = RankNpcNameMatch(left, foldedText);
                    const auto rightRank = RankNpcNameMatch(right, foldedText);
                    if (leftRank != rightRank) return leftRank < rightRank;
                }
                const auto primary = ComparePrimary(left, right, query);
                if (primary == 0) {
                    const auto byName = left.searchKeys.name.compare(right.searchKeys.name);
                    return query.ascending ? byName < 0 : byName > 0;
                }
                if (query.sort == SortKey::Distance &&
                    left.spatial.distance.has_value() != right.spatial.distance.has_value()) {
                    return primary < 0;
                }
                return query.ascending ? primary < 0 : primary > 0;
            });

        const auto total = matches.size();
        const auto normalizedLimit = std::clamp(query.limit, kMinimumResults, kMaximumResults);
        const auto visibleCount = query.unlimitedResults ? matches.size() :
            (std::min)(matches.size(), normalizedLimit);
        std::vector<NpcSnapshot> visible;
        visible.reserve(visibleCount);
        for (std::size_t index = 0; index < visibleCount; ++index) {
            auto npc = source[matches[index]];
            npc.favorite = IsFavorite(npc, query);
            visible.push_back(std::move(npc));
        }
        return SearchMatches{std::move(visible), total, textMatchTotal};
    }

    std::vector<std::string> SuggestNpcNames(
        RuntimeIndexView source,
        std::string_view text,
        std::size_t limit)
    {
        std::vector<std::string> suggestions;
        if (!source || !source->catalog || limit == 0) return suggestions;
        const auto query = FoldTextForSearch(text);
        const auto queryCodePoints = DecodeUtf8(query);
        if (!queryCodePoints || queryCodePoints->size() < 3 ||
            queryCodePoints->size() > 64) {
            return suggestions;
        }
        const std::size_t maximumDistance = queryCodePoints->size() < 6 ? 1 : 2;

        struct Candidate
        {
            std::string displayName;
            std::string foldedName;
            std::size_t distance;
        };
        std::vector<Candidate> candidates;
        for (const auto& npc : *source->catalog) {
            if (npc.displayName.empty() || !npc.IsUniqueBase()) continue;
            const auto& folded = npc.searchKeys.name;
            if (folded.empty() || folded == query) continue;
            const auto foldedCodePoints = DecodeUtf8(folded);
            if (!foldedCodePoints) continue;
            const auto lengthDifference = foldedCodePoints->size() > queryCodePoints->size() ?
                foldedCodePoints->size() - queryCodePoints->size() :
                queryCodePoints->size() - foldedCodePoints->size();
            if (lengthDifference > maximumDistance) continue;
            if (std::ranges::any_of(candidates, [&](const auto& candidate) {
                    return candidate.foldedName == folded;
                })) {
                continue;
            }
            const auto distance = DamerauLevenshtein(*queryCodePoints, *foldedCodePoints);
            if (distance <= maximumDistance) {
                candidates.push_back({npc.displayName, folded, distance});
            }
        }
        std::ranges::sort(candidates, [](const auto& left, const auto& right) {
            if (left.distance != right.distance) return left.distance < right.distance;
            return left.foldedName < right.foldedName;
        });
        const auto count = (std::min)(limit, candidates.size());
        suggestions.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            suggestions.push_back(std::move(candidates[index].displayName));
        }
        return suggestions;
    }
}
