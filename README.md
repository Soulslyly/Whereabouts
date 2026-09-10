# Whereabouts - Search, Locate, and Track NPCs

Whereabouts adds searchable NPC and location explorers to SKSE Menu Framework. It can locate named NPC references and cells, optionally include clearly labeled generic NPCs and location results in the main search, travel to selected cells, capture a console-selected or crosshair NPC, show useful details, run guarded NPC commands, save favorites and recent selections per save, and maintain up to 100 map-marker tracking aliases.

## Requirements

- Skyrim 1.5.97, 1.6.1170, or 1.7.104 (final 1.0.0 in-game acceptance is still pending on each runtime)
- Microsoft Visual C++ 2015-2022 x64 Redistributable
- SKSE matching that runtime
- Address Library for SKSE Plugins matching that runtime
- SKSE Menu Framework 3.14.1 or a later compatible major-3 release whose DLL fixed version is at least 3.14.x
- Backported Extended ESL Support (BEES), on Skyrim 1.5.97 only, for the unchanged modern light-plugin format

UIExtensions, MCM Helper, SkyUI MCM, and PapyrusUtil are not required.

Standard SKSE translation files are included for all nine Skyrim Special Edition languages. The Main archive ships clean English fallback text in every language file so translators can replace one file directly; optional machine translations are packaged separately. Whereabouts Settings can override its language without changing Skyrim's language, with a full restart required so the whole menu remains consistent.

## Features

- One search field for ranked names, case-insensitive exact runtime or plugin-local FormIDs such as `1a696`, `0001A696`, or `0x1A696`, and stable plugin/local IDs such as `Skyrim.esm:01A696`
- Invariant Unicode lowercase matching for names, plugins, and locations, with a byte-preserving fallback for malformed text
- A coherent immutable NPC catalog that initializes on the first menu open even when no console or crosshair target is selected
- Configurable live previews (50 rows by default) with a fixed expand/scroll footer, accurate match counts, and configurable full results (500 by default) after Search, Enter, or Virtual Keyboard Apply
- Optional Show all results removes both caps; disable it or lower the limits if a large modlist needs fewer results.
- A dedicated Locations page for cell names, EditorIDs, FormIDs, plugins, containing Location records, worldspaces, interior/exterior type, and exterior grid coordinates
- A clear Search content selector for NPCs only, NPCs and locations, or locations only, plus NPCs-first/locations-first mixed ordering and one shared configurable result cap
- Confirmed location travel through Skyrim's typed cell-loading path after the SKSE menu has closed; no console command, raw relocation, save record, or extra plugin record is used
- Live plugin suggestions plus location, same-location, state, follower, loaded, favorite, tracked, and generic-NPC filters
- Generic NPCs are labeled with their exact reference FormID and stay out of ordinary searches by default; Same location or Followers automatically includes them for that filter context unless the user turns them off
- Optional console-selected NPC capture when the framework opens, off by default, with independent crosshair fallback
- Explicit Use Console Target and Use Crosshair Target controls
- Independent, opt-in automatic console-target and crosshair-target selection; both default off
- Clear Selection control that does not immediately recapture the same unchanged console target, plus a visible Console, Crosshair, Search, Tracked, Favorites, or Recent source label
- Compact name, FormID, location, and plugin summary with color-coded state tags, Copy ID, and full NPC details in a closed section
- Separate Location, Cell, and Worldspace details with runtime FormIDs, EditorIDs, exterior cell grid coordinates, and current, last-observed, or unavailable location status; interiors are identified as interiors rather than an unknown worldspace
- Detailed or compact Location rows plus an immediate Copy menu for FormID, EditorID, stable plugin/local ID, or a ready-to-paste `coc EditorID` command
- Theme-aware alternating list rows, clipped-text hover expansion, and display-only compact colored-letter status badges with delayed explanations (`A/E/L` or `D/X/U`, plus contextual `T/F/G/M`)
- One Copy ID action menu: choose FormID, EditorID, or Stable and it copies immediately; unavailable EditorID/Stable choices fall back to FormID with a clear message
- Potential Follower Yes/No filter based on Skyrim's default potential-follower faction; this is descriptive and does not guarantee that recruitment dialogue is currently available
- Case-insensitive name, FormID, stable identity, and EditorID search with conservative submitted-search typo suggestions
- Unified whole-row NPC selection, Detailed/Compact result density, resizable/reorderable tables, Name/Status/Location header sorting, Random order, and a draggable results/details divider
- Travel, Bring, Inventory, Track, Enable/Disable, Select in Console, and Favorite commands
- Risk-based confirmations for cross-cell/worldspace movement and risky disabling; dead NPCs are never automatically resurrected
- Disabled-NPC relocation requires explicit confirmation, enables the NPC first, and can be cancelled
- Favorites and unique most-recent-first selections stored per save using stable plugin/local FormID identities
- Virtual Keyboard with Alphabetical and QWERTY layouts, live previews, and exact Cancel restoration
- Distance shown in meters by default, with feet and raw game-unit alternatives
- One tracking save-data warning per save, a persistent marker quest matching the original working lifecycle, and silent marker repair after load or a manual index refresh
- Dead tracked NPCs keep their marker while the body exists; after Skyrim makes a body unavailable, the marker slot is freed and a removable per-save missing-body record remains
- Tracked-death notifications use the selected Whereabouts language; the quest name is the Whereabouts proper name and each objective is the tracked NPC's alias name
- Silent suppression of only Whereabouts' initial Quest Started banner; the tracked NPC objective name and marker remain visible
- A non-blocking notice when a newly tracked NPC is in another worldspace and its map marker may not appear until that worldspace is entered
- Pause-safe Enable/Disable through Papyrus completion: Whereabouts keeps the current selection visible and verifies the observed actor state after gameplay resumes
- Optional More Informative Console label and lower-right details repaint after its real destination object reports ready; retries are frame-paced, bounded, and cancelled when the console closes, while ordinary console selection remains the fallback
- Missing-plugin and unavailable-NPC labels for saved entries, plus conservative one-click cleanup of entries from missing plugins
- Prepare for Uninstall cleanup that clears tracking aliases/markers, stops and resets the quest, clears Whereabouts save lists, and verifies completion before giving removal instructions
- Six uncluttered pages: Search, Locations, Tracked NPCs, Favorites, Recent, and Settings
- Delayed mouse-hover help only for non-obvious controls and exact reasons on unavailable commands

## Installation

Install the main archive with a mod manager and enable `Whereabouts.esp`. Keep the original Where Are You mod disabled when testing Whereabouts; their save data and settings are intentionally separate.

Open SKSE Menu Framework and choose the Whereabouts section. Use **Use Console Target** for an NPC selected by clicking it or using `prid`. To capture that NPC whenever Whereabouts opens, enable **Auto-select console target** in Settings.

## Configuration

Global settings are stored in `Data/SKSE/Plugins/Whereabouts.ini`. The Settings page keeps language, result limits, Virtual Keyboard, distance, tracking, and maintenance choices easy to reach and places uncommon safety and diagnostic controls under Advanced. `Follow Skyrim` is the default language; an explicit choice changes only Whereabouts after a full restart. Search content, result ordering, and filters remain transient menu-session choices. Favorites, recent history, and the one-time tracking warning acknowledgement are stored per save. Whereabouts adds no gameplay keybind; it is opened and navigated through SKSE Menu Framework.

## Save safety and removal

Whereabouts can be installed on an existing save. Favorites, Recent, the one-time warning acknowledgement, and missing-body history live only in the SKSE cosave and are harmless if their records are left behind. Active tracking uses quest aliases and should be cleaned before removal. Use **Settings > Prepare for Uninstall**, wait for the verified completion message, make a new manual save, exit Skyrim completely, and then remove the mod. A successfully cleaned, otherwise empty Whereabouts store writes no SKSE cosave records on that new save. If no NPC has ever been tracked, or every tracked NPC has already been untracked so no active aliases or markers remain, removal is expected to be low risk; the preparation button remains the safest route because it also stops and resets the resident tracking quest and clears Whereabouts save lists.

Prepare for Uninstall does not undo intentional Travel, Bring, Enable, or Disable changes made earlier.

**Resume Whereabouts** restarts and verifies tracking before unlocking commands. If verification fails, do not uninstall from that state; retry Resume or load a save from before cleanup. Reloading with Whereabouts still installed also resumes the mod, so uninstall by saving after successful cleanup and quitting without reloading.

## Building

Clone the repository with its pinned dependencies:

```powershell
git clone --recurse-submodules https://github.com/Soulslyly/Whereabouts.git
```

If the repository was cloned without that option, run `git submodule update --init --recursive`. GitHub's automatically generated source archives do not contain submodule contents; use the attached `Whereabouts-1.0.0-Source.zip` release asset for a self-contained source snapshot.

The native plugin uses CMake, Visual Studio 2022, vcpkg, the .NET 9 SDK, CommonLibSSE-NG v7.2.0, and the official SKSE Menu Framework consumer API. CommonLibSSE-NG and the framework API are pinned as Git submodules so their ownership and history remain explicit. Check out vcpkg commit `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4` at `.deps/vcpkg`, run its Windows bootstrap script, then configure and build with the supplied presets. `global.json` keeps PluginBuilder on .NET 9 with feature-band roll-forward. The release version is owned by `version.json`; required package versions and the vcpkg baseline are locked by `vcpkg.json`. The public Source archive intentionally excludes the internal developer test suite; its supplied CMake project detects that omission and builds the production targets normally.

Compile the four scripts in `papyrus/Source` with the official Papyrus compiler and matching Skyrim/SKSE imports. Put the matching SKSE source directory before the vanilla source directory so its extended declarations are authoritative. The typed Mutagen generator creates `Whereabouts.esp` independently with `dotnet run --project tools/PluginBuilder/Whereabouts.PluginBuilder.csproj -- build plugin/Whereabouts.esp`; no original or template plugin is required.

## Read-only Papyrus API

`WhereaboutsAPI` version 1 lets other mods query the currently published NPC catalog through optional Papyrus integration without changing Whereabouts state or performing direct game-form reads from the callback. Call `IsReady()` first and pass an NPC reference to the remaining queries. A not-ready catalog, stale save session, `None`, non-actor form, or unknown reference returns `False`, an empty string, or status `0`. The API exposes name and identity data, Location/Cell/Worldspace, numeric location status (`0` unavailable, `1` current, `2` last observed), and Alive, Enabled, Loaded, Follower, Potential Follower, Tracked, Favorite, and Generic state. It intentionally provides no mutation, event, subscription, or native C++ ABI. Consumers should cache results for their own operation instead of polling every scalar getter each frame.

See `docs/COMPATIBILITY.md` for the evidence-qualified runtime matrix. Whereabouts validates the already-loaded SMF DLL's fixed version and required exports before registering any menu callbacks. It does not resolve or call the framework's obsolete floating-point version export.

## Credits and permissions

Whereabouts is a new implementation. Thanks to k0mp1ex for Where Are You, an awesome mod and the inspiration for Whereabouts. No Where Are You or NPC Lookup assets or code are shipped as Whereabouts code. SKSE Menu Framework belongs to its respective author and is used through its public API under the included license notice.

The native plugin is built with CommonLibSSE-NG and depends at runtime on SKSE and Address Library for SKSE Plugins. The CommonLibSSE-NG license and Modding Exception are included in the main archive; exact upstream source information is included in the optional source archive. Credit belongs to their respective authors and maintainers.

Original Whereabouts material uses the open redistribution terms in `LICENSES/Whereabouts-Permissions.txt`. Third-party material retains its own terms; complete redistributable notices are included in `LICENSES/Third-Party-Notices.txt`. Where Are You was used as a structural reference only; no source or asset from it is distributed as Whereabouts code.
