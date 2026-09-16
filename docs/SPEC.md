# Whereabouts 1.1.0 Specification

## 1.1.0 parity-foundation scope

Search filter groups choose their column count from the available width, item spacing, and measured translated labels. Plugin and Location fields align in wide layouts and stack in narrow layouts; plugin suggestions use the available panel width. Result sorting presents an explicit Direction choice and disables that choice when Random makes direction meaningless. This phase preserves the 1.0.1 search-recovery behavior and makes no changes to search semantics, indexing, commands, tracking, serialization, settings schema, Papyrus, the public API, or the ESP.

## Goal

Create a clean Nexus-ready successor to Where Are You that searches, locates, inspects, commands, favorites, and tracks unique NPC references through SKSE Menu Framework without UIExtensions, MCM Helper, or a Whereabouts-specific gameplay keybind.

## Identity and compatibility

- Nexus title: **Whereabouts - Search, Locate, and Track NPCs**
- Runtime files use the `Whereabouts` prefix.
- The mod has new plugin, script, configuration, serialization, and log identities.
- It does not migrate `kxWhereAreYou` settings or tracking aliases.
- It must never be loaded alongside the original mod.
- Installing on an existing save is supported only when that save has no active conflicting original installation.

## Runtime targets and requirements

- Validate the universal build on Skyrim 1.5.97, 1.6.1170, and 1.7.104 before advertising each runtime; Skyrim 1.5.97 additionally requires BEES for the preserved HEDR 1.71 ESPFE.
- Prefer one CommonLibSSE-NG/Address Library DLL, but publish separate binaries if runtime testing proves that necessary.
- Direct requirements: matching SKSE and Address Library, plus exactly one supported menu framework: SKSE Menu Framework 3.14.1 or ApocryphaRealm Menu Framework 1.8.4 or a verified compatible successor within the accepted provider major line.
- Identify the loaded provider and validate its fixed file version plus the complete source-derived framework/ImGui export surface before constructing its event wrapper or registering pages. Accept stock SMF major 3 with minor 14 or later, or AMF major 1 at version 1.8.4 or later; fail closed on unknown providers, unreadable versions, unsupported major lines, or missing exports. Do not resolve or call the obsolete legacy float-version export.
- UIExtensions, MCM Helper, SkyUI MCM, and PapyrusUtil are not direct requirements.

## Architecture

- The native DLL owns SMF registration/rendering, cached search, target resolution, dynamic actor inspection, settings, favorites/recent serialization, dependency checks, and logging.
- The ESPFE owns one tracking quest, a player alias, 100 tracking aliases/objectives, and the VMAD connections required by the Papyrus bridge.
- Papyrus owns alias mutation, death events, quest-marker state, and any command whose exact verified Papyrus path is safer than a native equivalent.
- Tracking aliases are authoritative. Native serialization must not duplicate the active tracking list.
- All game objects are accessed on the game thread. Cached search data contains copied values, current-session runtime IDs, and stable identities where available, never long-lived raw actor pointers.
- One canonical NPC identity distinguishes the placed reference from its base actor and prevents generic actors sharing one base from collapsing into one result.
- Runtime search state is published as a shared immutable view containing one session, revision, readiness state, failure code, NPC catalog, and location catalog. Content-only NPC refreshes preserve the location catalog plus readiness/failure metadata; full rebuilds publish both catalogs coherently under one owner.
- SKSE Load and Revert callbacks invalidate the transient catalog before per-save records are processed. Stale-session views are rejected and stale rebuilds cannot publish into a newer session.

## SMF pages and formatting

- Register one top-level `Whereabouts` section with Search, Locations, Tracked NPCs, Favorites, Recent, and Settings pages.
- Render inside the standard Mod Control Panel and inherit its active theme, scaling, font, navigation tree, pause behavior, and gamepad focus behavior.
- Do not create a second full-screen application shell.
- The Search page uses Layout A: controls at top, results left, details/actions right, and stacked panes when width is insufficient.
- Use standard ImGui/SMF inputs, tables, full-cell row hit regions, buttons, popups, and Font Awesome icons. Do not reproduce UIExtensions wheel styling.
- Treat every informational cell and its blank space as one NPC-row selection target with one theme-derived hover/selection background. Keep action cells independent.
- Allow Search, Tracked NPCs, Favorites, and Recent columns to be resized and reordered for the current UI session. Provide Detailed and Compact list density.

## Search

- Rank normal search matches as exact, name prefix, word prefix, then substring.
- Use the same field for names, reference/base EditorIDs, exact runtime FormIDs, plugin-local FormIDs without a plugin name, and `Plugin.esm|esp|esl:localID` identities; rank exact runtime matches first, rank display-name matches ahead of EditorID-only matches, and never infer light-plugin semantics from a filename extension.
- After an explicit unfiltered ordinary-name search returns no results, offer at most three conservative typo suggestions. Never auto-correct or suggest for Preview, identity queries, active filters, short input, or distant terms.
- Live previews display up to the configured 10-100 row limit (50 by default); Search, Enter, and controller Apply display up to the configured 50-1000 row limit (500 by default). Total-match counts remain uncapped.
- Search named persistent NPC references and keep placed references distinct even when generic actors share one base.
- Sort the full matching set before applying the result cap.
- Filters start closed. Filters: plugin, alive/dead, enabled/disabled, current follower, loaded state, location, same location, favorites, and tracked state.
- Plugin filtering offers live case-insensitive suggestions from indexed plugin names.
- Valid UTF-8 names, plugins, and locations use invariant Unicode lowercase matching. Malformed text falls back to byte-preserving ASCII lowercase matching rather than failing search.
- Sorts: name, plugin, level, NPC location, loaded state, meaningful same-space distance, status, and transient random order. Search's NPC, Status, and Location headers map to the corresponding sorts. Random order advances only when it is selected or an explicit submitted Random search runs, so preview refreshes do not shuffle underneath the user.
- Rebuild the index after save load, on explicit Refresh Index, or once on first menu open when no ready catalog exists. Refresh visible/selected dynamic state without rescanning all forms every frame.
- Show a concise preparing or fixed failure message instead of a misleading zero-result state while the catalog is unavailable.

## Locations

- Build a transient catalog from stable, non-deleted `CELL` forms. Keep only copied strings, stable identity, runtime FormID, cell kind, optional exterior grid, containing Location label, and worldspace label; retain no raw form pointers.
- Search cell display names first, then EditorIDs, then containing Location/worldspace context. Accept the same runtime, plugin-local, and `Plugin.ext:localID` identity input as NPC search.
- Provide a dedicated sortable Locations page. Main Search exposes transient NPCs-only, NPCs-and-locations, and locations-only content choices; NPCs-only is the default. Mixed results can place NPCs or locations first and share one configured result cap.
- Detailed Location rows show secondary identity/context lines. Compact rows use one line and preserve the omitted EditorID, FormID, and containing-location context in delayed hover help.
- Selected Location details use the same fixed label alignment as NPC details. A stateless Copy menu immediately copies FormID, EditorID, stable plugin/local identity, or `coc EditorID`; an unavailable requested value falls back to FormID and reports the fallback.
- Always confirm location travel. Close SKSE Menu Framework first, re-resolve and identity-check the cell on the game task, then call CommonLibSSE-NG's typed `PlayerCharacter::CenterOnCell(TESObjectCELL*)` function.
- Location search and selection are transient. They add no INI field, SKSE serialization record, Papyrus change, quest alias, objective, ESP record, or uninstall step.

## Target acquisition

- When the default-off automatic console-target setting is enabled, capture a valid console-selected Actor once on each SMF open event.
- When the independent default-off automatic crosshair-target setting is enabled and no console target was captured, capture a valid crosshair Actor once.
- Otherwise retain the current menu-session selection.
- Provide explicit Use Console Target, Use Crosshair Target, and Clear Selection controls.
- Show the target source in the details pane.
- Resolve and validate the reference again immediately before every command.
- Player, non-actor, deleted, stale, and unavailable references are rejected.
- Select in Console publishes at most one retry task per SMF rendered frame, cancels on console close or lifecycle invalidation, rejects superseded requests, and expires only at its absolute two-second wall-clock deadline.
- More Informative Console repainting is optional. Invoke its callback only after the exact destination object exists and exposes `AddExtraInfo`; otherwise retain the vanilla selection without creating a hard dependency.

## Actor details

Display name, reference FormID, location, and plugin in the fixed compact summary. Put level, actor values, race, sex, meaningful Location/cell/worldspace values and identities, exterior cell grid coordinates, stable reference/base identities, Base NPC EditorID, state, follower and potential-follower status, favorite state, tracking state, and distance in a closed scrolling NPC Details section. Suppress duplicate or unavailable spatial rows and show a last-observed warning only when data is stale. Display interiors as `Interior` rather than implying a failed worldspace lookup. Display distance in meters by default, with feet and raw game units selectable. Copy identity through one `Copy ID` action menu: FormID, EditorID, or Stable copies immediately; EditorID means the Base NPC EditorID only, and an unavailable requested type falls back to FormID with a clear message.

## Public read API

`WhereaboutsAPI` version 1 is a hidden global Papyrus script backed only by the current immutable catalog snapshot. It rejects a not-ready catalog, stale session, `None`, non-actor form, and unknown reference with `False` or an empty string. It exposes scalar identity, location, and status queries only. It provides no writes, hooks, subscriptions, save records, native ABI, or automatic index rebuild. Consumers should cache results for their own operation instead of polling scalar getters every frame.

## Commands and safety

- Provide Travel to NPC, Move NPC to Player, Open Inventory, Track/Untrack, Enable/Disable, Select in Console, and Favorite/Unfavorite.
- Keep SMF open after safe inline commands, including Track/Untrack, Favorite, Bring, and Enable/Disable; close/release it before inventory, console, or player world-transition actions.
- Confirm disabling and bulk-clearing actions.
- Allow ordinary same-area movement without confirmation by default. Confirm different interior cells, different worldspaces, unknown location relationships, and user-enabled confirmation mode; use stronger handling for essential, protected, dead, or disabled actors.
- Never claim perfect quest involvement detection, silently resurrect an actor, or silently enable a disabled actor.
- When a target is disabled, require explicit enable-and-move confirmation or cancellation.
- Preserve configurable teleport separation distance: default 100, range 0-1000 game units.

## Tracking

- Support 100 active tracking aliases/objectives.
- Provide dashboard actions to inspect, travel, move, and untrack.
- Activate the tracking quest before displaying objectives and force marker redisplay for occupied aliases after load or index refresh.
- Wait a bounded number of game tasks for the quest script to bind before dispatching the first tracking call.
- Show the tracking/save-removal warning once per save before the first Track request. Untracking every NPC must not make that warning repeat.
- After the final alias clears through Untrack, Clear All, or unavailable-body retirement, hide its objectives and deactivate the quest without stopping or resetting it. Stop/reset is exclusive to Prepare for Uninstall.
- Notify when a tracked NPC dies by default, including the killer when safely available.
- Keep the marker and alias while the dead body exists, including while unloaded. Retire the objective only after the body is disabled, deleted, or otherwise genuinely unavailable; retain a removable missing-body history row without consuming a marker slot.
- Allow valid non-player actor references that the quest alias can safely retain. Clearly label generic/non-unique actors and preserve exact reference identity.

## Favorites and recent history

- Store plugin filename, local FormID, light/full plugin type, and last known name. A resolved runtime ID may exist only in the current-session cache and is never serialized.
- Resolve entries after load-order changes and preserve unavailable entries for review/removal.
- Label a row as Plugin missing when its owner plugin is absent, otherwise label an unresolved row as NPC unavailable; allow bulk removal of unavailable entries.
- Do not persist unrecoverable temporary references as favorites or recent-history entries.
- Record deliberate selections/actions in a unique most-recent-first per-save history, move repeats to the top, default to 20 entries, and allow clearing.

## Settings and input

- Keep language, Virtual Keyboard layout, list density, distance unit, tracking-death notification, index refresh, and Prepare for Uninstall on the default Settings surface.
- Language defaults to `Follow Skyrim`. An explicit language changes only Whereabouts, is stored globally in the INI, and applies after a full Skyrim restart so registered SKSE Menu Framework navigation and page content never mix languages.
- Keep live search, teleport separation, confirmations, menu-close behavior, and debug logging under a closed Advanced section.
- Keep version, dependencies, and credits under a closed About Whereabouts section.
- Add no Whereabouts-specific gameplay keybind. Opening and navigation remain owned by SKSE Menu Framework.
- All non-text controls must be keyboard and controller navigable.
- Provide a Virtual Keyboard with Alphabetical and QWERTY layouts, live preview, submitted search, and exact Cancel restoration; test it in-game before claiming full controller support.

## Persistence and performance

- Global settings, including the Whereabouts-only language override, use a small versioned configuration file written atomically.
- Favorites, recent history, tracked-death history, and the tracking-warning acknowledgement use versioned per-save SKSE serialization.
- Plugin/local FormID identity is preserved independently from resolved FormIDs.
- Build the form/reference index on the game thread; pure filtering may use copied cached data.
- Searches update only when inputs or filters change; submitted searches use the larger result window.
- Papyrus is event-driven and contains no update loop.

## Installation and removal safety

- Installing on an existing save is supported. The resident tracking quest is Start Game Enabled, but Whereabouts leaves its tracking aliases empty and objectives hidden until the player tracks an NPC. A completely empty/default Whereabouts store writes no namespaced SKSE records.
- Per-save SKSE records are namespaced and may be ignored safely when the DLL is absent.
- Active quest aliases are the meaningful removal concern. Prepare for Uninstall must block new commands, clear every marker/alias, stop/reset the quest, clear saved lists and acknowledgement state, then report success only after native verification.
- After verified preparation, instruct the player to make a new manual save, exit Skyrim completely, and remove the mod. Intentional movement and enable/disable changes are not reverted.
- Never describe mid-save removal as an absolute guarantee; the verified preparation workflow is the supported removal path.

## Packaging and publishing

- Main archive contains only `Whereabouts.esp`, `SKSE/Plugins/Whereabouts.dll`, the required INI and `Scripts/*.pex`, plus required legal notices.
- Exclude source, PDBs, logs, caches, tests, planning, validation, model metadata, machine paths, Git data, and temporary files.
- GitHub is the source distribution. Its pinned submodules, C++/Papyrus source, build configuration, public documentation, translation worksheet, and license notices remain separate from runtime downloads and exclude internal project-management records.
- `docs/TRANSLATING.md` and the translator worksheet are maintained in GitHub. Translation keys remain immutable; translators edit only the single tab-delimited value on the right.
- Credit k0mp1ex and state that Where Are You was used as a reference for the tracking-quest structure; retain its MIT notice. Credit SMF, CommonLibSSE-NG, SKSE, Address Library, and any additional assets actually used.
- Nexus upload only under the supplied permissions; no sale or paid redistribution.

## Acceptance gates

- Exact native build and Papyrus compile succeed against local dependencies.
- Plugin header, master chain, ESL allocation, aliases, objectives, and VMAD validate structurally.
- Search/ID parsing, duplicate names, sorting-before-cap, console/crosshair capture, command safety/completion, death tracking, serialization, missing plugins, uninstall state, and Virtual Keyboard behavior receive targeted tests.
- Runtime smoke tests pass on 1.5.97, 1.6.1170, and 1.7.104 before those runtimes are advertised.
- The final Main and optional Translation archives pass content allowlists, version-string checks, and SHA-256 recording; the staged GitHub source manifest passes its separate cleanliness check.
- Structural or automated checks are never reported as user-confirmed runtime testing.
