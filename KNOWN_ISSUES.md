# Known Issues and Limitations

There are currently no confirmed unresolved bugs listed for Whereabouts 1.0.1. The behaviors below are known limitations of Skyrim, external UI mods, or the information available to Whereabouts.

## Tracking Across Worldspaces

A tracked NPC in another worldspace may not appear on the map until you enter that worldspace or reach a map that can display it. The **Tracked NPCs** page keeps the NPC listed and explains when the marker may not be visible.

Disabled or otherwise unavailable NPCs may only have a last-known location. Their marker cannot provide a new live position until Skyrim makes the reference available again.

## Location Information

Whereabouts can only show location, cell, and worldspace information that Skyrim or another installed mod has assigned to the reference. Some mod-added interiors and worldspaces provide little useful naming data, so a result may be broad, repeated, or unknown.

## Console Extensions

**Select in Console** changes the actual console target used by commands. Some console-information extensions cache parts of their display and may continue showing details from the previous target even after the underlying selection changes.

## Translations and Fonts

The optional non-English files are machine-translated starting points and may contain awkward or incorrect wording. Community corrections are welcome.

Chinese, Japanese, and other non-Latin text also require the active interface font or theme to contain those glyphs. Missing glyphs can appear as question marks even when the translation file is valid.

## Skyrim 1.5.97

Skyrim 1.5.97 requires [Backported Extended ESL Support](https://www.nexusmods.com/skyrimspecialedition/mods/106441) because `Whereabouts.esp` uses the modern extended light-plugin record range. The ESP must also be enabled; the DLL can load and search while tracking remains unavailable if the quest-bearing ESP is disabled.

## Reporting Something New

Please check [Support](SUPPORT.md) and open a [bug report](https://github.com/Soulslyly/Whereabouts/issues/new?template=bug_report.md). A report is most useful when it includes the exact Skyrim, SKSE, and SKSE Menu Framework versions plus `Whereabouts.log`.
