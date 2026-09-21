#pragma once

#include "Core/FormIdentity.h"

#include <cstdint>
#include <string>

namespace whereabouts
{
    struct RecordedCellSnapshot
    {
        FormIdentity identity;
        std::uint32_t runtimeFormID{0};
        std::string displayName;
        std::string editorID;
        std::string worldspaceName;
        bool interior{false};

        [[nodiscard]] bool Known() const noexcept
        {
            return identity.IsPersistable();
        }

        [[nodiscard]] bool operator==(const RecordedCellSnapshot&) const noexcept = default;
    };
}
