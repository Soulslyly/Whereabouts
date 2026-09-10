# Whereabouts - Search, Locate, and Track NPCs

This is my take on the [NPC Lookup](https://www.nexusmods.com/skyrimspecialedition/mods/43097) / [Where Are You](https://www.nexusmods.com/skyrimspecialedition/mods/76063) type of mod. Whereabouts lets you search for NPCs, followers, and locations, giving you options to find missing NPCs, track them with a vanilla quest, teleport them to you, or travel to them. You can even grab stats and levels from a generic NPC you're struggling with.

Those are just some of the things you might use it for. Whereabouts covers a ton of features, so see if anything tickles your little fancy.

**Project links:** [Releases](https://github.com/Soulslyly/Whereabouts/releases) · [Changelog](CHANGELOG.md) · [Roadmap](ROADMAP.md) · [Known issues](KNOWN_ISSUES.md) · [Get help](SUPPORT.md) · [Papyrus API](docs/PAPYRUS_API.md) · [Translations](docs/TRANSLATING.md)

## Main Features

- Search NPCs and locations
- Get NPC stats and information easily
- Locate and track NPCs, both in the menu and with a vanilla quest
- Run useful commands on NPCs
- Built from the ground up around quality-of-life features

## Other Features

- Live search, typo suggestions, and plugin autocomplete
- Name, FormID, EditorID, and stable plugin-ID searches
- NPC-only, location-only, and combined searches
- Nearby, current-follower, and potential-follower discovery
- Optional generic NPC support
- Current and last-known location information
- Up to 100 tracked NPCs
- Dead-body tracking and missing-body history
- Favorites and Recent NPCs stored per save
- Missing-plugin and unavailable-reference handling
- Colored NPC status indicators
- Crosshair and optional console-target selection
- Controller support and a virtual keyboard
- Resizable and reorderable columns
- Compact and detailed layouts
- Copyable NPC and location identities
- Prepare-for-uninstall and Resume controls
- Translation-ready interface
- Read-only Papyrus API for other mods

## FAQ

### Why can't I see a tracked marker or NPC?

The NPC might be in another worldspace with no direct way there. Check the **Tracked NPCs** page; most of the time it will tell you what is happening.

### Is it safe to uninstall mid-game?

It can be. Open **Settings** and select **Prepare for Uninstall**. Wait until Whereabouts says cleanup was verified, make a new manual save, exit Skyrim completely, and then uninstall the mod.

You'll probably also be fine if you never use tracking, because everything else is stored in the SKSE cosave and cannot keep running after the mod is removed. Prepare for Uninstall is still the safest option because it clears tracking, Favorites, Recent, and other Whereabouts save data.

### Can I install it mid-game?

Yes.

### How do I open Whereabouts?

Open SKSE Menu Framework—F1 by default—and select **Whereabouts**. A big reason I made this mod was to avoid taking up another gameplay keybind.

### Which game versions are supported?

Skyrim 1.5.97, 1.6.1170, and 1.7.104 are supported. Skyrim 1.5.97 also needs BEES for the modern light plugin.

## Requirements

Make sure every requirement matches your installed Skyrim version.

- [Skyrim Script Extender](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) 3.14.1 or a later compatible major-3 release
- [Backported Extended ESL Support](https://www.nexusmods.com/skyrimspecialedition/mods/106441) — Skyrim 1.5.97 only
- [Microsoft Visual C++ 2015–2022 x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)

UIExtensions, MCM Helper, SkyUI MCM, and PapyrusUtil are not required.

## Installation

Install `Whereabouts-1.0.0-Main.zip` with your mod manager and enable `Whereabouts.esp`. Open SKSE Menu Framework and choose Whereabouts.

The optional Translations archive contains machine-translated starting points for every non-English language supported by Skyrim. It is not required for English users.

## Author Note & AI Disclosure

Whereabouts exists because I wanted my own take on the “Where Are You” style of NPC-finding mod. My C++ knowledge is limited, so AI was used to write the code while I handled the ideas, design, testing, feedback, and revisions. I tested and refined the result extensively in game, and everything I release will clearly disclose AI use, include its source code, and stay open-permissions just like everything else I've made.

If AI mods aren't for you, I get it. Please keep the comments respectful and avoid unnecessary drama. Thanks for reading my small amount of yap.

## Documentation

- [Papyrus API](docs/PAPYRUS_API.md) — read-only NPC information for compatibility patches and other mods
- [Translating Whereabouts](docs/TRANSLATING.md) — format, encoding, installation, testing, and CJK font support
- [Compatibility](docs/COMPATIBILITY.md) — runtime and dependency details
- [Dependency lock](docs/DEPENDENCIES.md) — exact build versions and upstream sources

## Community

Found a bug or have an idea? You can use [GitHub Issues](https://github.com/Soulslyly/Whereabouts/issues), leave a comment on Nexus, or just follow the project quietly. GitHub is optional; you do not need an account here to use the mod.

- Check [Known Issues](KNOWN_ISSUES.md) before reporting a problem.
- Read [Support](SUPPORT.md) for the small amount of information that makes a report useful.
- See the [Roadmap](ROADMAP.md) for planned work and ideas under investigation.
- See [Contributing](CONTRIBUTING.md) if you want to submit a translation, patch, or API improvement.

## Building

Clone the repository with its pinned dependencies:

```powershell
git clone --recurse-submodules https://github.com/Soulslyly/Whereabouts.git
```

If you cloned it without that option, run:

```powershell
git submodule update --init --recursive
```

GitHub's automatically generated source archives do not include submodule contents, so cloning recursively is the recommended build path.

The native plugin uses CMake, Visual Studio 2022, vcpkg, the .NET 9 SDK, CommonLibSSE-NG v7.2.0, and the official SKSE Menu Framework consumer API. CommonLibSSE-NG and the framework API are pinned as Git submodules so their ownership and history remain clear.

Check out vcpkg commit `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4` at `.deps/vcpkg`, run its Windows bootstrap script, then configure and build with the supplied presets. Compile the four scripts in `papyrus/Source` with the official Papyrus compiler and matching Skyrim/SKSE imports.

The typed Mutagen generator creates `Whereabouts.esp` independently:

```powershell
dotnet run --project tools/PluginBuilder/Whereabouts.PluginBuilder.csproj -- build plugin/Whereabouts.esp
```

No original or template plugin is required.

## Credits & Thanks

- Skyrim Script Extender — Ian Patterson, Stephen Abel, Paul Connelly, Brendan Borthwick, and the whole SKSE team
- Address Library for SKSE Plugins — meh321
- SKSE Menu Framework and its consumer API — Thiago / SkyrimThiago and QTR-Modding contributors
- Backported Extended ESL Support — Nukem; required only on Skyrim 1.5.97
- CommonLibSSE-NG / CommonLibVR NG branch — alandtse and contributors
- Bethesda Game Studios — The Elder Scrolls V: Skyrim Special Edition and its Creation Kit/Papyrus tools

Reference only, but still a huge help and the reason this mod is here:

- [Where Are You - Lookup And Track Followers and other NPCs](https://www.nexusmods.com/skyrimspecialedition/mods/76063) by k0mp1ex — design and vanilla tracking-quest reference
- [NPC Lookup](https://www.nexusmods.com/skyrimspecialedition/mods/43097) by doticu / r-neal-kelly — feature and workflow inspiration

No Where Are You or NPC Lookup code or assets are included.

## Permissions

All of my mods are **free to use, modify, and upload** anywhere, as long as you credit me as the original creator.

The only exception is when my mod includes assets made by other authors—in those cases, you must also credit the original creators and/or gain permission from them.

Any other questions about permissions, just DM me and ask. Third-party components included with Whereabouts remain under their own licenses.
