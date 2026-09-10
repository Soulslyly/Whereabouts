#include "Persistence/RuntimeSettingsState.h"

namespace whereabouts
{
    RuntimeSettingsState::RuntimeSettingsState(Settings settings) :
        settings_(std::make_shared<const Settings>(std::move(settings)))
    {}

    std::shared_ptr<const Settings> RuntimeSettingsState::Snapshot() const noexcept
    {
        return settings_.load(std::memory_order_acquire);
    }

    void RuntimeSettingsState::Publish(const Settings& settings)
    {
        settings_.store(std::make_shared<const Settings>(settings), std::memory_order_release);
    }
}
