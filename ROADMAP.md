# Whereabouts Roadmap

This is the public direction for Whereabouts. It is not a promise that every idea will ship, and there are no release dates until an update is actually ready.

## Current Release

Version 2.1.0 expands Whereabouts from a live-actor lookup tool into an NPC record and reference explorer. It can index many unique NPCs before their cells have loaded, clearly represent record-only NPCs as unavailable, expose recorded-cell and plugin provenance data, offer contextual Travel to Cell and session-only Return, and refresh live availability without reopening the menu.

The current release also includes the full-width NPC Inspector, template-aware record filters, Essential and Protected controls, custom commands, configurable quick actions, cross-save Favorites, integrated translations, SMF/AMF provider support, separate Standard and experimental VR builds, and density-aware layouts for large catalogs. Visible-row clipping and event-driven refreshes keep large searches responsive.

## Next Maintenance Priorities

Near-term work will be driven by reproducible reports from 2.1.0 rather than another large feature expansion. Priorities are:

- Runtime compatibility reports across supported Skyrim, SKSE, Address Library, SMF, and AMF combinations
- Record-only, recorded-cell, quest-gated, disabled, dynamic, and unusual placed-NPC edge cases
- Search and menu performance on very large load orders
- Narrow-window layout, controller navigation, and long-translation corrections
- Skyrim VR feedback before the experimental build can be treated as ordinary support

## Investigating

These ideas need a safe, useful, and compatible design before they become planned work:

- Better map and location interaction without fragile camera hooks or assumptions about custom worldspaces
- Compatibility improvements for unusual cells, worldspaces, followers, templates, and generated NPC records
- Small additions to the read-only Papyrus API when another mod has a concrete use case
- Additional quality-of-life actions that remain single-target, discoverable, and compact

## Ideas Bin

An idea appearing here has not been accepted for a release.

- Additional NPC metadata and filters that remain readable at every density
- Optional import, export, or sharing tools for stable NPC identity lists
- More read-only ways for other mods to consume Whereabouts data without gaining unsafe actor control
- Suggestions submitted through Nexus or [GitHub Issues](https://github.com/Soulslyly/Whereabouts/issues)

## Not Planned

- Force-spawning or cloning replacement actors for NPCs Skyrim has not instantiated
- Automatic multi-NPC commands or mutations without an explicit per-NPC action
- A separate gameplay hotkey for Whereabouts
- Replacing SKSE Menu Framework or ApocryphaRealm Menu Framework with UIExtensions or an MCM
- Fragile map-camera hooks likely to break custom worldspaces or map replacers
