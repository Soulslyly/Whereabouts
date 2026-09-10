#include "PCH.h"

#include "Compatibility/MenuFrameworkRuntime.h"

#include <array>
#include <vector>

namespace whereabouts
{
    namespace
    {
        std::optional<MenuFrameworkVersion> ReadFixedVersion(HMODULE module) noexcept
        {
            try {
                std::wstring path(32768, L'\0');
                const auto length = GetModuleFileNameW(
                    module,
                    path.data(),
                    static_cast<DWORD>(path.size()));
                if (length == 0 || length >= path.size()) return std::nullopt;
                path.resize(length);

                DWORD unused = 0;
                const auto byteCount = GetFileVersionInfoSizeW(path.c_str(), &unused);
                if (byteCount == 0) return std::nullopt;
                std::vector<std::byte> bytes(byteCount);
                if (!GetFileVersionInfoW(path.c_str(), 0, byteCount, bytes.data())) {
                    return std::nullopt;
                }

                VS_FIXEDFILEINFO* info = nullptr;
                UINT infoSize = 0;
                if (!VerQueryValueW(
                        bytes.data(),
                        L"\\",
                        reinterpret_cast<void**>(std::addressof(info)),
                        std::addressof(infoSize)) ||
                    !info || infoSize < sizeof(VS_FIXEDFILEINFO) ||
                    info->dwSignature != 0xFEEF04BD) {
                    return std::nullopt;
                }

                return MenuFrameworkVersion{
                    HIWORD(info->dwFileVersionMS),
                    LOWORD(info->dwFileVersionMS),
                    HIWORD(info->dwFileVersionLS),
                    LOWORD(info->dwFileVersionLS)};
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    MenuFrameworkRuntimeProbe ProbeLoadedMenuFramework() noexcept
    {
        MenuFrameworkRuntimeProbe result;
        const auto module = GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!module) return result;

        result.moduleLoaded = true;
        result.fixedVersion = ReadFixedVersion(module);

        constexpr std::array requiredExports{
            "RegisterEventPriority",
            "UnregisterEvent",
            "AddSectionItem",
            "GetMainWindow",
            "igBeginChild_ID",
            "igBeginChild_Str",
            "igBeginCombo",
            "igBeginDisabled",
            "igBeginPopupModal",
            "igBeginTable",
            "igButton",
            "igCalcTextSize",
            "igCheckbox",
            "igCloseCurrentPopup",
            "igCollapsingHeader_BoolPtr",
            "igCollapsingHeader_TreeNodeFlags",
            "igEndChild",
            "igEndCombo",
            "igEndDisabled",
            "igEndPopup",
            "igEndTable",
            "igGetColorU32_Col",
            "igGetColorU32_U32",
            "igGetColorU32_Vec4",
            "igGetContentRegionAvail",
            "igGetCursorScreenPos",
            "igGetFrameHeightWithSpacing",
            "igGetIO",
            "igGetMainViewport",
            "igGetScrollMaxY",
            "igGetScrollY",
            "igGetStyle",
            "igGetStyleColorVec4",
            "igGetTextLineHeightWithSpacing",
            "igInputText",
            "igInvisibleButton",
            "igIsItemActivated",
            "igIsItemActive",
            "igIsItemDeactivatedAfterEdit",
            "igIsItemEdited",
            "igIsItemHovered",
            "igIsItemClicked",
            "igOpenPopup_ID",
            "igOpenPopup_Str",
            "igPopID",
            "igPopStyleColor",
            "igPushID_Int",
            "igPushID_Ptr",
            "igPushID_Str",
            "igPushID_StrStr",
            "igPushStyleColor_U32",
            "igPushStyleColor_Vec4",
            "igSameLine",
            "igSelectable_Bool",
            "igSelectable_BoolPtr",
            "igSeparator",
            "igSeparatorText",
            "igSetClipboardText",
            "igSetCursorScreenPos",
            "igSetMouseCursor",
            "igSetNextItemWidth",
            "igSetNextWindowPos",
            "igSetTooltipV",
            "igSliderFloat",
            "igSliderInt",
            "igSmallButton",
            "igSpacing",
            "igTableGetRowIndex",
            "igTableGetSortSpecs",
            "igTableHeadersRow",
            "igTableNextColumn",
            "igTableNextRow",
            "igTableSetBgColor",
            "igTableSetColumnIndex",
            "igTableSetColumnSortDirection",
            "igTableSetupColumn",
            "igTextColoredV",
            "igTextDisabledV",
            "igTextUnformatted",
            "igTextV",
            "igTextWrappedV"};
        result.requiredExportsAvailable = true;
        for (const auto* name : requiredExports) {
            if (GetProcAddress(module, name)) continue;
            result.requiredExportsAvailable = false;
            result.missingRequiredExport = name;
            break;
        }

        return result;
    }
}
