#pragma once

#include <functional>
#include <utility>

namespace whereabouts
{
    template <class Work, class Result, class ExceptionSink>
    [[nodiscard]] Result GuardCallback(
        Work&& work,
        Result fallback,
        ExceptionSink&& exceptionSink) noexcept
    {
        try {
            return std::invoke(std::forward<Work>(work));
        } catch (...) {
            try {
                std::invoke(std::forward<ExceptionSink>(exceptionSink));
            } catch (...) {}
            return fallback;
        }
    }

    template <class Work, class ExceptionSink>
    void GuardCallbackVoid(Work&& work, ExceptionSink&& exceptionSink) noexcept
    {
        try {
            std::invoke(std::forward<Work>(work));
        } catch (...) {
            try {
                std::invoke(std::forward<ExceptionSink>(exceptionSink));
            } catch (...) {}
        }
    }
}
