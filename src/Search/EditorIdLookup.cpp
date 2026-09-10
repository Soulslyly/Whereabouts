#include "Search/EditorIdLookup.h"

namespace whereabouts
{
    void EditorIdLookup::Observe(std::uint32_t runtimeFormID, std::string_view editorID)
    {
        if (runtimeFormID == 0 || editorID.empty()) return;

        auto [position, inserted] = values_.try_emplace(runtimeFormID, editorID);
        if (!inserted && editorID < position->second) position->second = editorID;
    }

    std::string EditorIdLookup::Find(std::uint32_t runtimeFormID) const
    {
        const auto position = values_.find(runtimeFormID);
        return position == values_.end() ? std::string{} : position->second;
    }

    std::size_t EditorIdLookup::Size() const noexcept
    {
        return values_.size();
    }
}
