#pragma once

namespace whereabouts
{
    class AppContext;

    [[nodiscard]] AppContext* AcquireProcessContext() noexcept;
}
