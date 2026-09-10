#include "Core/SpatialPresentation.h"

#include <format>

namespace whereabouts
{
    std::string PrimarySpatialLabel(const SpatialSnapshot& spatial)
    {
        if (!spatial.location.empty()) return spatial.location;
        if (!spatial.cell.empty()) return spatial.cell;
        if (!spatial.worldspace.empty()) return spatial.worldspace;
        if (spatial.kind == SpatialKind::Interior) return "Interior cell";
        if (spatial.kind == SpatialKind::Exterior) return "Exterior cell";
        return "Location unavailable";
    }

    std::string SecondaryWorldspaceLabel(const SpatialSnapshot& spatial)
    {
        if (spatial.worldspace.empty()) return {};
        return spatial.worldspace == PrimarySpatialLabel(spatial) ? std::string{} : spatial.worldspace;
    }

    std::string SearchableSpatialText(const SpatialSnapshot& spatial)
    {
        auto text = PrimarySpatialLabel(spatial);
        const auto secondary = SecondaryWorldspaceLabel(spatial);
        if (!secondary.empty()) text += " " + secondary;
        for (const auto* editorID : {
                 &spatial.locationEditorID,
                 &spatial.cellEditorID,
                 &spatial.worldspaceEditorID}) {
            if (!editorID->empty()) text += " " + *editorID;
        }
        return text;
    }

    std::string PreferredWorldspaceName(
        std::string_view actorWorldspace,
        std::string_view cellWorldspace)
    {
        return std::string(actorWorldspace.empty() ? cellWorldspace : actorWorldspace);
    }

    std::string WorldspaceSpatialLabel(const SpatialSnapshot& spatial)
    {
        if (!spatial.worldspace.empty()) return spatial.worldspace;
        if (spatial.kind == SpatialKind::Interior) return "Interior";
        return "Unavailable";
    }

    std::string SpatialIdentityLabel(
        std::string_view editorID,
        std::uint32_t runtimeFormID)
    {
        if (!editorID.empty() && runtimeFormID != 0) {
            return std::format("{} / FormID {:08X}", editorID, runtimeFormID);
        }
        if (!editorID.empty()) return std::string(editorID);
        return runtimeFormID == 0 ? "Unavailable" : std::format("FormID {:08X}", runtimeFormID);
    }

    std::string ExteriorCellGridLabel(const SpatialSnapshot& spatial)
    {
        if (!spatial.exteriorCellX || !spatial.exteriorCellY) return "Unavailable";
        return std::format("X {}, Y {}", *spatial.exteriorCellX, *spatial.exteriorCellY);
    }

    std::string_view LocationSourceLabel(LocationSource source) noexcept
    {
        switch (source) {
        case LocationSource::Actor: return "Actor";
        case LocationSource::Cell: return "Cell";
        case LocationSource::NamedParent: return "Named parent";
        case LocationSource::None: return "Unavailable";
        }
        return "Unavailable";
    }

    std::string_view SpatialFreshnessLabel(SpatialFreshness freshness) noexcept
    {
        switch (freshness) {
        case SpatialFreshness::Current: return "Current";
        case SpatialFreshness::LastObserved: return "Last observed";
        case SpatialFreshness::Unavailable: return "Unavailable";
        }
        return "Unavailable";
    }
}
