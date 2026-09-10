# Whereabouts 1.0.0 Compatibility Matrix

Whereabouts uses one Address Library/CommonLibSSE-NG DLL for the supported Skyrim runtime lines. A row marked **runtime pending** is structurally supported but must not be advertised as verified until the final candidate passes the full in-game matrix in `TESTING.md`.

| Skyrim | SKSE | Address Library | SKSE Menu Framework | Evidence | Status |
|---|---|---|---|---|---|
| 1.5.97 | 2.0.20 | Exact `version-1-5-97-0.bin` / format 1 | Exact installed 3.14.0.0 DLL | Universal DLL declaration, exact format-1 database, complete SMF export surface, and exact Papyrus imports pass; BEES is required for the unchanged HEDR 1.71 ESPFE; final Whereabouts runtime test still required | Runtime pending |
| 1.6.1170 | 2.2.8 | v12 / format 2 | 3.14.1 package, DLL fixed version 3.14.x | Universal DLL build path and format-2 fixture pass; exact profile runtime test still required | Runtime pending |
| 1.7.104 | 2.3.1 | v13 / format 5 | 3.14.1 package, DLL fixed version 3.14.0.0 | Universal DLL build path, format-5 support, installed 80-export surface, and dependency boot evidence pass; final Whereabouts runtime test still required | Runtime pending |
| Any | Matching | Matching | Package 3.8.0, installed DLL fixed version 3.0.0.0 | Exact installed legacy DLL is below the supported fixed-version floor | Rejected at startup |
| Any | Matching | Matching | Unreadable version, unknown major, or missing required exports | Startup probe fails closed before Whereabouts registers menu callbacks | Unsupported |

## Runtime dependency policy

- The loaded `SKSEMenuFramework.dll`, not a file merely present on disk, is authoritative.
- Supported SMF DLL fixed versions are major version 3 with minor version 14 or later. A future major version is not assumed compatible.
- All 80 framework/ImGui functions used by the current build must be exported before the framework wrapper is used. This includes the four registration/command bootstrap functions and the UI surface derived from the shipped source.
- The legacy floating-point version export is neither resolved nor called; it does not participate in compatibility decisions or logging.
- The exact installed 3.14.0.0 DLL currently exposes the complete framework/ImGui export surface used by 1.0.0. The validator derives that list from the shipped source and consumer header.

## Optional More Informative Console support

More Informative Console is optional. Without its Scaleform callback, Select in Console retains the ordinary Skyrim selection path. When the callback is present, Whereabouts waits across rendered frames for MIC's real destination object and its `AddExtraInfo` member, refreshes once, and stops after a bounded deadline. Compatibility with the installed MIC versions remains an in-game acceptance item.
