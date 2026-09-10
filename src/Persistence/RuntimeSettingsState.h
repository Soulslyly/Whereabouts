#pragma once

#include "Persistence/Settings.h"

#include <atomic>
#include <memory>

namespace whereabouts
{
    class RuntimeSettingsState
    {
    public:
        explicit RuntimeSettingsState(Settings settings);

        [[nodiscard]] std::shared_ptr<const Settings> Snapshot() const noexcept;
        void Publish(const Settings& settings);

    private:
        std::atomic<std::shared_ptr<const Settings>> settings_;
    };
}
