# Whereabouts 1.1.0 Compatibility Matrix

> **EXPERIMENTAL SKYRIM VR BUILD — UNTESTED IN-GAME**
> The separate `1.1.0-vr.1` artifact is tool-validated only and is not part of the stable universal DLL. It targets Skyrim VR 1.4.15 with SKSEVR 2.0.12, VR Address Library 0.109.0+, Skyrim VR ESL Support 1.3.2+, and SKSE Menu Framework 3.14.x. It belongs under Nexus Optional Files until user testing establishes runtime support.

Whereabouts uses one Address Library/CommonLibSSE-NG DLL for the supported Skyrim runtime lines. A row marked **runtime pending** is structurally supported but must not be advertised as verified until the final candidate passes the full in-game matrix in `TESTING.md`.

| Skyrim | SKSE | Address Library | SKSE Menu Framework | Evidence | Status |
|---|---|---|---|---|---|
| 1.5.97 | 2.0.20 | Exact `version-1-5-97-0.bin` / format 1 | Exact installed 3.14.0.0 DLL | Universal DLL declaration, exact format-1 database, complete SMF export surface, and exact Papyrus imports pass; BEES is required for the unchanged HEDR 1.71 ESPFE; final Whereabouts runtime test still required | Runtime pending |
| 1.6.1170 | 2.2.6 | v12 / format 2 | 3.14.1 package, DLL fixed version 3.14.x | Universal DLL build path and format-2 fixture pass; exact profile runtime test still required | Runtime pending |
| 1.7.104 | 2.3.1 | v13 / format 5 | 3.14.1 package, DLL fixed version 3.14.0.0 | Universal DLL build path, format-5 support, installed 81-export surface, and dependency boot evidence pass; final Whereabouts runtime test still required | Runtime pending |
| Any | Matching | Matching | Package 3.8.0, installed DLL fixed version 3.0.0.0 | Exact installed legacy DLL is below the supported fixed-version floor | Rejected at startup |
| Any | Matching | Matching | Unreadable version, unknown major, or missing required exports | Startup probe fails closed before Whereabouts registers menu callbacks | Unsupported |
| Skyrim VR 1.4.15 | SKSEVR 2.0.12 | VR Address Library 0.109.0+ plus Skyrim VR ESL Support 1.3.2+ | 3.14.x VR-capable DLL | VR-only Release compile and static API checks; no headset or in-game evidence | Experimental / runtime untested |

## Runtime dependency policy

- The loaded `SKSEMenuFramework.dll`, not a file merely present on disk, is authoritative.
- Supported SMF DLL fixed versions are major version 3 with minor version 14 or later. A future major version is not assumed compatible.
- All 81 framework/ImGui functions used by the current build must be exported before the framework wrapper is used. This includes the four registration/command bootstrap functions and the UI surface derived from the shipped source.
- The legacy floating-point version export is neither resolved nor called; it does not participate in compatibility decisions or logging.
- The exact installed 3.14.0.0 DLL currently exposes the complete framework/ImGui export surface used by the 1.1.0 development build. The validator derives that list from the shipped source and consumer header.
- Skyrim VR ESL Support's documented Engine Fixes VR and `MaxStdio` setup remains a user-side requirement. Whereabouts does not install or modify either dependency.

## Read-only API compatibility

Papyrus API v2 preserves every v1 name and signature. Consumers negotiate with `GetVersion()` or `SupportsVersion()` and must call `IsReady()` before catalog queries. Version 2 adds only copied immutable values; it adds no native C++ ABI, tasklet-callable callback, mutable command, serialization record, plugin record, or runtime dependency. See `API.md` for the exact contract.

## Optional More Informative Console support

More Informative Console is optional. Without its Scaleform callback, Select in Console retains the ordinary Skyrim selection path. When the callback is present, Whereabouts waits across rendered frames for MIC's real destination object and its `AddExtraInfo` member, refreshes once, and stops after a bounded deadline. Compatibility with the installed MIC versions remains an in-game acceptance item.
