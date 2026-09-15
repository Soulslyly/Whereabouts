#include "PCH.h"

#include "Compatibility/VrEnvironmentCompatibility.h"

namespace whereabouts
{
    namespace
    {
        constexpr auto kPluginFile = "Whereabouts.esp";
    }

    VrEnvironmentProbe ProbeLoadedVrEnvironment() noexcept
    {
#if defined(EXCLUSIVE_SKYRIM_VR)
        auto* dataHandler = RE::TESDataHandler::GetSingleton(true);
        return VrEnvironmentProbe{
            .dataHandlerAvailable = dataHandler != nullptr,
            .lightPluginLoaded = dataHandler &&
                dataHandler->LookupLoadedLightModByName(kPluginFile) != nullptr};
#else
        return VrEnvironmentProbe{
            .dataHandlerAvailable = true,
            .lightPluginLoaded = true};
#endif
    }
}
