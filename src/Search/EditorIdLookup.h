#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace whereabouts
{
    class EditorIdLookup
    {
    public:
        void Observe(std::uint32_t runtimeFormID, std::string_view editorID);
        [[nodiscard]] std::string Find(std::uint32_t runtimeFormID) const;
        [[nodiscard]] std::size_t Size() const noexcept;

    private:
        std::unordered_map<std::uint32_t, std::string> values_;
    };
}
