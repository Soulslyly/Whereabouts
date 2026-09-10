#include "Tracking/TrackingQuestLayout.h"

#include <array>
#include <limits>

namespace whereabouts
{
    std::expected<TrackingQuestLayoutResult, std::string> ValidateTrackingQuestLayout(
        const TrackingQuestLayoutInput& input)
    {
        if (input.aliases.size() != kTrackingSlotCount + 1) {
            return std::unexpected("Tracking quest must contain exactly 101 aliases");
        }
        if (input.objectives.size() != kTrackingSlotCount) {
            return std::unexpected("Tracking quest must contain exactly 100 objectives");
        }

        TrackingQuestLayoutResult result;
        result.aliasVectorIndices.fill(std::numeric_limits<std::size_t>::max());

        for (std::size_t vectorIndex = 0; vectorIndex < input.aliases.size(); ++vectorIndex) {
            const auto& alias = input.aliases[vectorIndex];
            if (alias.aliasID < kFirstTrackingAliasID ||
                alias.aliasID >= kFirstTrackingAliasID + kTrackingSlotCount) {
                continue;
            }
            if (!alias.referenceAlias) {
                return std::unexpected("A tracking alias is not a reference alias");
            }

            const auto slot = static_cast<std::size_t>(alias.aliasID - kFirstTrackingAliasID);
            if (result.aliasVectorIndices[slot] != std::numeric_limits<std::size_t>::max()) {
                return std::unexpected("Tracking quest contains a duplicate tracking alias ID");
            }
            result.aliasVectorIndices[slot] = vectorIndex;
        }

        for (const auto vectorIndex : result.aliasVectorIndices) {
            if (vectorIndex == std::numeric_limits<std::size_t>::max()) {
                return std::unexpected("Tracking quest is missing a required tracking alias ID");
            }
        }

        std::array<bool, kTrackingSlotCount> objectives{};
        for (const auto objective : input.objectives) {
            if (objective >= objectives.size() || objectives[objective]) {
                return std::unexpected("Tracking quest objective indices must be unique values from 0 to 99");
            }
            objectives[objective] = true;
        }
        for (const auto present : objectives) {
            if (!present) {
                return std::unexpected("Tracking quest is missing a required objective index");
            }
        }

        return result;
    }
}
