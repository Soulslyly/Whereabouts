#include "Persistence/RuntimeSettingsState.h"

namespace whereabouts
{
    RuntimeSettingsState::RuntimeSettingsState(Settings settings) :
        settings_(nullptr)
    {
        settings.Normalize();
        settings_.store(
            std::make_shared<const Settings>(std::move(settings)),
            std::memory_order_release);
    }

    std::shared_ptr<const Settings> RuntimeSettingsState::Snapshot() const noexcept
    {
        return settings_.load(std::memory_order_acquire);
    }

    void RuntimeSettingsState::Publish(const Settings& settings)
    {
        auto normalized = settings;
        normalized.Normalize();
        settings_.store(
            std::make_shared<const Settings>(std::move(normalized)),
            std::memory_order_release);
    }
}
