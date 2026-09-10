#pragma once

#include <cstddef>

namespace whereabouts
{
    struct QuestCleanupObservation
    {
        bool stopped{false};
        bool running{false};
        std::size_t activeAliasCount{0};
        bool objectivesHidden{false};
    };

    [[nodiscard]] constexpr bool IsQuestCleanupVerified(
        const QuestCleanupObservation& observation) noexcept
    {
        return observation.stopped && observation.activeAliasCount == 0 &&
               observation.objectivesHidden;
    }
}
