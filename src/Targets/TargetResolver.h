#pragma once

#include "Core/NpcIdentity.h"
#include "Core/NpcSnapshot.h"
#include "Persistence/RuntimeSettingsState.h"
#include "Lifecycle/RequestSerialGate.h"
#include "Targets/TargetSelection.h"

#include <RE/Skyrim.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace whereabouts
{
    class RuntimeIndex;

    struct SelectedTarget
    {
        NpcIdentity identity;
        std::string displayName;
        RE::ActorHandle handle;
        TargetSource source{TargetSource::None};

        [[nodiscard]] std::uint32_t ReferenceRuntimeID() const noexcept
        {
            return identity.ReferenceRuntimeID();
        }
    };

    class TargetResolver
    {
    public:
        TargetResolver(RuntimeIndex& index, const RuntimeSettingsState& settings) noexcept;

        [[nodiscard]] bool CaptureOnMenuOpen();
        [[nodiscard]] std::uint64_t IssueSelectionRequest() noexcept;
        [[nodiscard]] bool IsSelectionRequestCurrent(std::uint64_t serial) const noexcept;
        [[nodiscard]] bool UseConsoleTarget();
        [[nodiscard]] bool UseCrosshairTarget();
        [[nodiscard]] bool Select(RE::Actor& actor, TargetSource source);
        [[nodiscard]] bool Select(const NpcSnapshot& snapshot, TargetSource source);
        void Clear() noexcept;
        void ClearAndSuppressConsoleTarget() noexcept;

        [[nodiscard]] RE::NiPointer<RE::Actor> ResolveCurrent() const;
        [[nodiscard]] std::optional<SelectedTarget> Current() const;

    private:
        [[nodiscard]] static RE::NiPointer<RE::Actor> ConsoleActor();
        [[nodiscard]] static RE::NiPointer<RE::Actor> CrosshairActor();
        [[nodiscard]] static bool IsValidActor(const RE::Actor* actor);

        RuntimeIndex& index_;
        const RuntimeSettingsState& settings_;
        mutable std::mutex currentMutex_;
        std::optional<SelectedTarget> current_;
        ConsoleTargetSuppression consoleSuppression_;
        RequestSerialGate selectionGate_;
    };
}
