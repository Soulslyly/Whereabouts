# Whereabouts 1.0.0 Dependency Lock

## Local build environment

- Operating system: Windows x64
- Visual Studio: Community 2022 17.14.30 (`17.14.37203.1`)
- MSVC toolset: `14.44.35207`
- CMake: `4.4.0`
- .NET SDK: `9.0.315`, selected by `global.json` from the `9.0.300` feature band
- Generator: Visual Studio 17 2022, x64
- Python: `3.12.15`
- Existing `VCPKG_ROOT`: unset

The PluginBuilder targets `net9.0`. `global.json` allows later .NET 9 feature bands but prevents the host's .NET 10 SDK from being selected implicitly. Translation generation, build helpers, tests, and release validation are compatible with Windows PowerShell 5.1; the non-ASCII translation generator is stored as UTF-8 with a BOM so Windows PowerShell decodes it correctly.

The packaged DLL uses the dynamic Microsoft C/C++ runtime. Players therefore need the Microsoft Visual C++ 2015-2022 x64 Redistributable installed in addition to the Skyrim-side requirements below.

## Development runtime evidence

- Legacy target: `1.5.97.0` / SKSE `2.0.20` / exact format-1 `version-1-5-97-0.bin`; exact SMF 3.14.0.0 and BEES load evidence is present, but Whereabouts runtime acceptance is pending
- Preserved Skyrim target: `1.6.1170.0` / SKSE `2.2.8` / Address Library v12 format 2
- Current Skyrim target: `1.7.104.0` / SKSE `2.3.1` / exact `versionlib-1-7-104-0.bin`
- The current SKSE 2.3.1 loader log identifies runtime `01070680` and records a complete boot into the test profile.
- SKSE Menu Framework package metadata reports `3.14.1.0`; its DLL metadata and runtime log self-report `3.14.0.0`. The installed binary loaded correctly under SKSE 2.3.1.

Installed game and mod-manager paths are read-only evidence and are intentionally omitted from public release archives.

## Pinned source authorities

- SKSE Menu Framework 3 consumer API: `https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API.git`
  - Commit: `1dcb70179076aae4ab626f43c5baab2735ca5877`
  - License: LGPL-2.1
- SKSE Menu Framework implementation audited for current-runtime support: `https://github.com/QTR-Modding/SKSE-Menu-Framework-3.git`
  - Commit: `c97cdce6dd207c7cf1401611bc82bd7e8f97a814`
  - Project version: `3.14.0.0`; Nexus package: `3.14.1.0`
- CommonLibSSE-NG source authority: `https://github.com/alandtse/CommonLibVR.git`, branch `ng`
  - Version: `7.2.0`
  - Commit: `7a60f4de794095d7b0f8928d1b930a52e9a7da83`
  - License: GPL-3.0-or-later with the Modding Exception in `EXCEPTIONS.md`
  - Git checkouts pin it as the `external/CommonLibSSE-NG` submodule. The downloadable Source ZIP includes the same exact source tree for a self-contained rebuild.
- Microsoft vcpkg baseline: `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`

## Runtime targets

- Legacy release test: Skyrim `1.5.97` / SKSE `2.0.20` / Address Library all-in-one legacy database / BEES
- Development and first semantic test: Skyrim `1.6.1170` / SKSE `2.2.8` / Address Library all-in-one v12
- Current-runtime release test: Skyrim `1.7.104` / SKSE `2.3.1` / Address Library all-in-one v13
- Required menu framework for all targets: SKSE Menu Framework `3.14.1` or a later compatible major-3 release. Whereabouts checks the loaded DLL fixed version and complete required-export surface and does not register below fixed version `3.14.x`; it does not resolve or call the obsolete float-version export.
- The official SKSE site was rechecked on 2026-09-04 and identifies Steam runtime `1.7.104` with SKSE `2.3.1` as the current Anniversary Edition pair.
- One DLL declares both ordinary Address Library and Address Library v5 support. Runtime support is advertised only after in-game testing on each target.

## Compatibility evidence

- The 1.6.1170 Address Library table begins with format version 2.
- The available 1.7.99 Address Library table begins with format version 5, the database family used by the 1.7 runtime line.
- Vendored CommonLibSSE-NG v7.2.0 dispatches formats 1, 2, and 5 and includes upstream test fixtures for 1.6.1170 format 2 and 1.7.99 format 5.
- Whereabouts exports `SKSEPlugin_Version` with `kVersionIndependentEx_AddressLibraryV5`, ordinary Address Library use, no-struct independence, and minimum SKSE 2.0.20.
- The exact 1.5.97 setup contains SkyrimSE 1.5.97, SKSE 2.0.20, `version-1-5-97-0.bin`, SMF 3.14.0.0, and BEES. Its SKSE log proves those dependencies load together; Whereabouts itself still needs the final-hash runtime matrix there.
- SMF 3.14.1 statically identifies CommonLibSSE-NG 6.7.1 and contains the format-5 parser. Its official Nexus changelog says version 3.14 added Skyrim 1.7 compatibility and 3.14.1 corrected the release build.
- SMF's license-file validation source is compiled only when `VALIDATE_LICENSE` is enabled; the public release build forces that option off.

## API observations

- The pinned SMF header declares `SetSection`, `AddSectionItem`, `AddEvent`, `AddInputEvent`, `GetMainWindow`, and a legacy float-version function. Whereabouts neither resolves nor calls that legacy function; compatibility uses the DLL fixed version and the complete required-export probe.
- The installed SMF 3.14.0.0 DLL passes the current derived 80-export contract covering every ImGui wrapper currently called by Whereabouts plus its critical framework exports. Its SHA-256 is `FE7F398B62DDF23D2EA4C163A0EBFA2BFF80D19B7462224F037F79FA057EA67A`.
- The owner-only exact-binary path is supplied through the disposable `WHEREABOUTS_SMF_DLL` CMake cache value. It is never written into source or release archives.
- SMF render and event callbacks use `__stdcall`; drawing calls are under `ImGuiMCP`.
- The pinned header is byte-for-byte identical to the current official API repository copy.
- The pinned SMF header has no documented API to open the main framework on a specified page. The optional direct Search shortcut therefore remains inactive.
- CommonLibSSE-NG v7.2.0 provides the runtime form, console selection, crosshair target, serialization, and task interfaces used by Whereabouts.
- `TESObjectREFR::GetParentCell`, `GetSaveParentCell`, `GetCurrentLocation`, `GetWorldspace`, `GetPosition`, and `BGSLocation::parentLoc` are the pinned spatial authorities used during game-thread capture. Whereabouts copies their derived values and retains none of those pointers in the published catalog.
- SMF registration waits for SKSE's `kDataLoaded` lifecycle message. Open-event capture is queued through `SKSE::TaskInterface` before reading game forms.

## Independent plugin build

`tools/PluginBuilder` creates the complete ESPFE from explicit typed records. It does not open or copy an original/template plugin:

```powershell
dotnet run --project tools/PluginBuilder/Whereabouts.PluginBuilder.csproj -- build plugin/Whereabouts.esp
dotnet run --project tools/PluginBuilder/Whereabouts.PluginBuilder.csproj -- validate plugin/Whereabouts.esp
```

The generator fixes the plugin name, HEDR 1.71 light allocation, Skyrim.esm master, quest local FormID `0x801`, quest EditorID, journal title and description, 101 aliases, 100 objectives, and all VMAD bindings. The original Where Are You mod remains a credited design reference only.

## Release verification

Use the supplied Release checklist and production validators for each rebuilt archive. Compiler success and structural plugin validation do not prove game behavior. The 1.0.0 candidate requires final-hash in-game acceptance on all three runtime targets, especially tracking restart and journal notifications. No runtime claim follows from the dependency matrix alone.
