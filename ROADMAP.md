# Whereabouts Roadmap

This is the public direction for Whereabouts. It is not a promise that every idea will ship, and there are no release dates until an update is actually ready.

## Current Maintenance Release

Version 1.0.1 improves filter clarity and missing-result recovery. It labels every Any/Yes/No status filter, shows the number of active filter categories, ignores surrounding search whitespace, and offers explicit filter-clearing or index-refresh recovery when a submitted search finds nothing visible.

## Planned Next

The next feature update is expected to focus on better ways to narrow NPC results:

- Race filtering
- Sex filtering
- Essential/protected filtering
- Interior/exterior filtering
- Worldspace filtering

The goal is to add these without turning the Search page into a wall of controls. Exact layout and combinations still need in-game UI testing.

## Investigating

These are useful ideas, but they need a safe and compatible implementation before they become planned work:

- Better map and location interaction, including opening the map from an NPC or location when Skyrim can represent it safely
- Better presentation when third-party console extensions cache information from an older target
- Compatibility improvements for mods that add unusual cells, worldspaces, followers, or location data
- Small additions to the read-only Papyrus API when another mod has a real use case for them

## Ideas Bin

Larger suggestions stay here until their usefulness and technical cost are clear. An idea appearing here does not mean it has been accepted for a release.

- Additional NPC metadata and filters that remain readable in both compact and detailed layouts
- More ways to use Whereabouts information from other mods without giving external mods unsafe control over actors
- Quality-of-life improvements suggested through Nexus or [GitHub Issues](https://github.com/Soulslyly/Whereabouts/issues)

## Not Planned

- A separate gameplay hotkey for Whereabouts
- Replacing SKSE Menu Framework with UIExtensions or an MCM
- Automatic actor changes performed without a clear user command
- Fragile map-camera hooks that are likely to break custom worldspaces or map replacers
