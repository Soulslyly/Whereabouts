# Whereabouts 2.0.0 Compatibility Matrix

Whereabouts uses one Address Library/CommonLibSSE-NG DLL for the supported Skyrim runtime lines. A row marked **runtime pending** is structurally supported but must not be advertised as verified until the final candidate passes the full in-game matrix in `TESTING.md`.

| Skyrim | SKSE | Address Library | Menu framework | Evidence | Status |
|---|---|---|---|---|---|
| 1.5.97 | 2.0.20 | Exact `version-1-5-97-0.bin` / format 1 | Exact installed 3.14.0.0 DLL | Universal DLL declaration, exact format-1 database, complete SMF export surface, and exact Papyrus imports pass; BEES is required for the unchanged HEDR 1.71 ESPFE; final Whereabouts runtime test still required | Runtime pending |
| 1.6.1170 | 2.2.6 | v12 / format 2 | 3.14.1 package, DLL fixed version 3.14.x | Universal DLL build path and format-2 fixture pass; exact profile runtime test still required | Runtime pending |
| 1.7.104 | 2.3.1 | v13 / format 5 | 3.14.1 package, DLL fixed version 3.14.0.0 | Universal DLL build path, format-5 support, installed 88-export surface, and dependency boot evidence pass; final Whereabouts runtime test still required | Runtime pending |
| 1.6.1179 GOG | Matching | Matching | AMF 1.8.4.0 | Exact installed AMF DLL passes the same source-derived required-export surface as stock SMF; the provider-aware correction from 1.1.1 is retained in 2.0.0 | Runtime pending |
| 1.4.15 VR | 2.0.12 | VR Address Library plus Skyrim VR ESL Support | One supported SMF-compatible provider | The unified VR Release target compiles against the pinned CommonLib/OpenVR source and fails closed on unsupported environment evidence; no in-game VR test has been performed | Experimental; unconfirmed |
| Any | Matching | Matching | Package 3.8.0, installed DLL fixed version 3.0.0.0 | Exact installed legacy DLL is below the supported fixed-version floor | Rejected at startup |
| Any | Matching | Matching | Unreadable version, unknown major, or missing required exports | Startup probe fails closed before Whereabouts registers menu callbacks | Unsupported |

## Runtime dependency policy

- The loaded provider, not a framework file merely present on disk, is authoritative. Enable exactly one of SMF or AMF.
- Supported stock SMF DLL fixed versions are major 3 with minor 14 or later. Supported AMF DLL fixed versions are major 1 with version 1.8.4 or later. Future major versions are not assumed compatible.
- Every framework/ImGui function used by the current build must be exported before the framework wrapper is used. This includes the four registration/command bootstrap functions and the UI surface derived from the shipped source.
- The legacy floating-point version export is neither resolved nor called; it does not participate in compatibility decisions or logging.
- Exact installed SMF 3.14.0.0 and AMF 1.8.4.0 DLLs expose the complete 88-export framework/ImGui surface used by 2.0.0. The validator derives that list from the shipped source and consumer header.

## Read-only API compatibility

Papyrus API v2 preserves every v1 name and signature. Consumers negotiate with `GetVersion()` or `SupportsVersion()` and must call `IsReady()` before catalog queries. Version 2 adds only copied immutable values; it adds no native C++ ABI, tasklet-callable callback, mutable command, serialization record, plugin record, or runtime dependency. See `API.md` for the exact contract.

## Optional More Informative Console support

More Informative Console is optional. Without its Scaleform callback, Select in Console retains the ordinary Skyrim selection path. When the callback is present, Whereabouts waits across rendered frames for MIC's real destination object and its `AddExtraInfo` member, refreshes once, and stops after a bounded deadline. Compatibility with the installed MIC versions remains an in-game acceptance item.
