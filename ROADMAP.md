# Whereabouts Roadmap

This is the public direction for Whereabouts. It is not a promise that every idea will ship, and there are no release dates until an update is actually ready.

## Current Release

Version 1.1.0 adds template-aware Essential and Protected filtering, compact common and Advanced filter sections, searchable Plugin/Faction/Base Keyword pickers, hybrid Location filtering, Super Compact rows, redesigned Selected NPC details, and searchable paginated Recent/Favorites/Tracked lists.

## Planned Next

The next maintenance work will be driven by concrete reports from the 1.1.0 release. Priority goes to reproducible runtime compatibility, translation, layout, and saved-list usability issues rather than adding another large feature set immediately.

## Investigating

These are useful ideas, but they need a safe and compatible implementation before they become planned work:

- Better map and location interaction, including opening the map from an NPC or location when Skyrim can represent it safely
- Better presentation when third-party console extensions cache information from an older target
- Compatibility improvements for mods that add unusual cells, worldspaces, followers, or location data
- Small additions to the read-only Papyrus API when another mod has a real use case for them

## Ideas Bin

Larger suggestions stay here until their usefulness and technical cost are clear. An idea appearing here does not mean it has been accepted for a release.

- Additional NPC metadata and filters that remain readable in Detailed, Compact, and Super Compact layouts
- More ways to use Whereabouts information from other mods without giving external mods unsafe control over actors
- Quality-of-life improvements suggested through Nexus or [GitHub Issues](https://github.com/Soulslyly/Whereabouts/issues)

## Not Planned

- A separate gameplay hotkey for Whereabouts
- Replacing SKSE Menu Framework with UIExtensions or an MCM
- Automatic actor changes performed without a clear user command
- Fragile map-camera hooks that are likely to break custom worldspaces or map replacers
