# Whereabouts Testing

## Candidate under test

Use the final-hash 1.1.1 Main candidate for runtime passes and compare its SHA-256 with the release validator. Install 1.1.1 as a separate test mod in a disposable profile and save. Retained runtime pairs are Skyrim 1.5.97/SKSE 2.0.20 with BEES, Skyrim 1.6.1170/SKSE 2.2.6, and Skyrim 1.7.104/SKSE 2.3.1. Every profile needs the matching Address Library, an enabled `Whereabouts.esp`, and exactly one supported menu framework: SMF 3.14.x+ within major 3 or AMF 1.8.4+ within major 1. Dependency boot evidence is not Whereabouts runtime acceptance.

Before runtime installation, validate all four Main-archive PEX files with `tools/normalize-pex-metadata.ps1 -ValidateOnly`. This proves only that compiler timestamp, user, and computer header fields are canonical; it does not prove script behavior in Skyrim.

## Dependency startup gate

1. With supported SMF 3.14 enabled by itself, boot to a disposable save and confirm `Whereabouts.log` names SKSE Menu Framework, records fixed version `3.14.x`, accepts compatibility, and reports runtime capability ready before the index rebuild.
2. Replace SMF with supported AMF 1.8.4 by itself and repeat. Confirm the log names ApocryphaRealm Menu Framework, records fixed version `1.8.4.x`, accepts compatibility, and reports runtime capability ready.
3. In a disposable profile only, verify an unsupported provider/version is rejected with one clear compatibility log and no Whereabouts pages, index rebuild, console sink activity, or crash.
4. Confirm no private dependency path or installed framework DLL appears in either release archive.

## Automated checks

Configure and build the Debug or Release preset, then run its matching CTest preset. Pure tests cover canonical identity, name/FormID/stable-ID/EditorID search, conservative typo suggestions, spatial presentation, settings, serialization codecs, command policy, tracking lifecycle, row interaction, and UI models. `IndexRuntimeTests.cpp` additionally compiles and executes the same `RuntimeIndex.cpp` and `IndexCoordinator.cpp` used by the DLL, with only SKSE scheduling, tracked-alias capture, marker repair, and game-form capture supplied at their external boundaries. It deterministically covers request coalescing, session/readiness publication, first-open state decisions, scheduler and capture failures, stale build/failure rejection, in-flight invalidation, immutable old views, marker-repair intent, targeted publication, and EditorID retention. Structural tests separately cover Address Library formats 2 and 5, the exported SKSE compatibility declaration, plugin records, UI source contracts, and archive policy. None of these tests proves SMF/SKSE callback timing or player-visible behavior inside Skyrim.

## Runtime target capture

Run these cases on a disposable test profile and save:

1. With **Auto-select console target** at its default off value, select a named NPC in the console and open Whereabouts. The console NPC must not replace the current Whereabouts selection.
2. Enable **Auto-select console target**, click a named NPC in the console, close it, then open Whereabouts. Repeat with `prid`; each valid console NPC must become the selected target.
3. Leave a stale or deleted console selection and aim at a named NPC. With crosshair fallback enabled, the crosshair NPC must be selected.
4. Select a non-actor reference in the console while aiming at a named NPC. The non-actor must be rejected and the crosshair NPC used.
5. Aim at a valid generic actor. The actor must be accepted, clearly labeled Generic, and show its exact reference FormID. Aim at the player and confirm the player is rejected.
6. Open the framework with no valid console or crosshair actor and no prior selection. The target panel must show no selection without errors.
7. Select an NPC from Search, close the framework, clear the console/crosshair target, and reopen it. The deliberate selection must be retained.
8. Use the explicit Console Target and Crosshair Target controls. Each must replace the current target only when its source resolves to a valid NPC reference, and the displayed source label must match.
9. With automatic console capture enabled and an NPC still selected in the console, use Clear Selection, close and reopen the framework, and confirm the same console NPC is not recaptured. Select a different console NPC and confirm capture resumes; clear the console selection and confirm suppression resets.
10. Save and reload with favorites and recent entries present. Resolvable identities must reconnect to their live references and the previous save's selected target must not carry over.
11. Reload after removing a plugin represented in favorites or recent history. Its stable entry must remain removable and must not resolve to another form.

## Localization loading

The Main archive contains 392 clean English fallback rows under every supported language filename. Each row has an unchanged `$Whereabouts_*` key, exactly one required tab, and the editable English value on the right.

With `Follow Skyrim (default)`, boot from a fully closed process and confirm `Whereabouts.log` reports Skyrim's configured language and 392 catalog entries. Choose an explicit language in Whereabouts Settings, quit Skyrim completely, restart, and confirm Whereabouts uses that file without changing Skyrim's own language. The Settings page must show the loaded language. Changing the choice during the same process must keep the current UI language consistent and show `Restart Skyrim to apply this language.`

## Responsive filter layout and sort direction

1. At 1280x720, 1920x1080, and an ultrawide resolution, resize the SKSE Menu Framework panel from its practical minimum to a wide layout. Plugin and Location must stack without clipping when narrow and share one aligned row when wide.
2. Repeat with English and one language whose translated filter labels are visibly longer. State and context controls must reduce their column count before labels overlap, clip, or push adjacent controls out of reach.
3. Repeat with the default theme and two installed alternate themes. Hover, selection, alternating rows, controls, and labels must remain legible; Whereabouts must not impose theme-specific background colors.
4. Open plugin suggestions in narrow and wide layouts. The suggestion list must match the available filter-panel width, remain usable, and close after a plugin is selected.
5. Select Name, Status, and Location sorts from both the Result order control and the corresponding result-table headers. Ascending and Descending must agree between both interaction paths.
6. Select Random. Direction must become disabled and read `Not used`; its delayed help must explain that Random has no direction. Switch back to a directional sort and confirm the last chosen direction remains usable.
7. Smoke-test Search, selection, Favorite, Track/Untrack, Travel, Bring, and Prepare for Uninstall/Resume to confirm this layout-only phase did not change their behavior.

Repeat once with the selected file intentionally absent in a disposable test install. Whereabouts must remain readable in compiled English, must not crash, and must log that English is being used for that process. Restore the file before continuing ordinary testing.

## Runtime indexing

After New Game and Post Load Game, confirm the log reports an index count and Search contains named persistent NPC references. Generic NPCs must be excluded from ordinary search by default, labeled when included, and unsafe transient/generated actors must not enter the global index. Dynamic references may be selected and commanded while valid, but must be refused by favorites and serialized recent history when they have no plugin/local FormID identity.

From the main menu, enter a disposable test cell and open Whereabouts before selecting or aiming at any NPC. Search must show `Preparing search index...` if capture has not finished and then populate without closing/reopening the menu. Repeated opens during preparation must coalesce into one full build and must not replay quest-start or tracked-objective notifications.

Switch between two disposable saves with visibly different searchable NPC sets, opening Whereabouts close to each load boundary. After Load/Revert invalidation, a new search must not show the previous save's catalog and an older queued build, command, selection, tracking mutation, Papyrus completion, or UI completion must not affect the new save.

Test accented Latin and, when the installed load order provides them, Cyrillic or Greek names/plugin/location labels using different letter case. Confirm ordinary search and plugin suggestions agree. Test an exterior actor, interior actor, named cell, unnamed cell if available, unloaded actor, and unavailable actor. Location, Cell, Worldspace, their record identities, exterior grid, and Current/Last observed/Unavailable labels must remain semantically distinct. Verify Potential Follower Yes and No against known examples and do not treat it as proof that recruitment dialogue is currently available.

## Location search and travel

1. Open Whereabouts without a console or crosshair target. Confirm Search and Locations populate after one index build and no tracked objective notification replays.
2. On Locations, search `Whiterun`, a known cell EditorID, an eight-digit runtime FormID, the same cell's local FormID, and `Plugin.ext:localID`. Confirm identity matching is case-insensitive and results remain stable after sorting Name, Plugin, and Worldspace both directions.
3. Confirm the default limits show at most 50 rows while typing and 500 after Enter/Search. Set the minimums (10/50) and maximums (100/1000), leave Settings, repeat both paths, and verify the Show more/refine footer matches the visible and total counts.
4. On Search, test NPCs only, NPCs and locations, and Locations only. In mixed mode, test NPCs first and Locations first. Confirm plugin/location filters affect every included catalog, excluded catalogs disappear immediately after a live refresh or submitted Search, and the displayed rows never exceed the selected shared limit.
5. Select a location from each page. Confirm the whole row is clickable and Details show Cell, EditorID, Plugin, FormID, containing Location, Worldspace, Type, and exterior grid when available. Toggle Detailed/Compact and confirm compact Location rows remain one line while delayed hover text exposes omitted identity/context.
6. From selected Location details, copy FormID, EditorID, Stable, and COC command. Confirm each click copies immediately, the menu remains open, and COC reads `coc EditorID`. Test a location without an EditorID and confirm COC/EditorID falls back to FormID with a visible explanation.
7. Cancel location travel and confirm the menu stays open and the player does not move. Confirm travel, verify the menu closes before loading begins, and arrive in the selected cell without a console command appearing.
8. Test one vanilla interior, one vanilla exterior, one DLC cell, and one custom-mod cell. Record failure text for any cell Skyrim refuses. Confirm a removed-plugin/stale selection fails closed after an index refresh instead of traveling through a reused runtime FormID.
9. Resize the SKSE Menu Framework from narrow to wide. The Search field, five target/action buttons, filter controls, and Location details must remain reachable without clipping; all removed filter subheadings must remain absent.
10. Repeat a successful interior and exterior travel on Skyrim 1.5.97, 1.6.1170, and 1.7.104. Static relocation evidence does not qualify a runtime until these cases pass in game.

## Command acceptance

Use a disposable save and test each command against an ordinary living NPC, a teammate, a dead NPC, a disabled NPC, an essential/protected NPC, and an unloaded NPC where applicable.

- Travel and Bring preserve the configured 0-1000 unit separation.
- With Serana unloaded in another valid location, confirm Bring once, wait for her to arrive, then repeat Bring after she is loaded nearby. Neither request may crash or issue a second movement before the first completes; `Whereabouts.log` must contain matching `Movement request` and successful `Movement completion` entries.
- Reproduce Travel to disabled Serana in Dimhollow on a disposable save. Confirm Whereabouts requires Enable and move, logs `settle true`, closes before the latent settle pass, places the player near Serana without an underground result, and does not crash. Repeat Travel after Serana is enabled and loaded, then repeat with another cross-cell NPC and a same-area NPC.
- Bring must log `settle false` and remain single-pass even when the NPC began in another cell or worldspace.
- A dead NPC is refused and is not resurrected.
- A disabled NPC offers Enable and move or Cancel; no unsafe move-without-enabling path is present.
- An unloaded NPC in the same exterior worldspace or the same interior cell does not prompt when Confirm teleport is off.
- A different interior cell, different worldspace, or unknown location relationship produces its specific confirmation when Confirm teleport is off.
- Name, FormID, location, and plugin remain visible above the command buttons.
- NPC Details starts closed and reports actor values, race, sex, cell, worldspace, reference/base identity, current follower state, favorite, tracking, and same-space distance.
- Distance defaults to meters and switches correctly to feet and game units from Settings without changing distance-sort order.
- Inventory opens only for a loaded actor.
- Enable/Disable requests confirmation, warns for essential/protected actors, keeps Whereabouts open under the default inline-command setting, and verifies the actual state after Papyrus completes. Test both directions and confirm the button changes to the observed opposite action.
- First select Aela in the console so More Informative Console displays her, close it, select Serana in Whereabouts, then use Select in Console. The console must open on Serana; both MIC's upper-left identity and lower-right details must refresh from Aela to Serana. The log must show frame-separated progress rather than all retries in one timestamp. Repeat with the installed MIC version on each runtime profile.
- Immediately close the console after Select in Console. Pending work must cancel without reopening it, touching a later console selection, or logging continued retries.
- Issue Select in Console for two different NPCs in quick succession. Only the newest request may win; the older request must not repaint or clear the newer NPC's MIC details.
- Repeat Select in Console with More Informative Console disabled. Vanilla selection must still target the requested NPC and finish without waiting for an optional overlay.
- Favorite refuses a dynamic identity and survives save/reload for stable identities.
- Track toggles one authoritative alias and never exceeds 100 active markers.
- Track, Untrack, Bring, Enable, and Disable keep Whereabouts open under the default inline-command setting. Enable and Disable must show the correct opposite action after completion.

## Tracking and persistence

1. On a save that has never tracked an NPC, Track one NPC. Confirm the save/removal warning appears once, Continue and Track creates a visible world/map objective marker, and Cancel makes no change.
2. Track a second NPC, untrack both, and then track another. The warning must not repeat anywhere in that save.
3. Confirm the Tracked NPCs page, Search tracked filter, world/map markers, and active count agree after every Track/Untrack without reopening the menu.
4. Untrack the final NPC and clear all markers; confirm objectives and aliases clear without a quest stop/reset log. Track another NPC afterward and confirm the resident quest displays the new map/compass marker without repeating the save-data warning.
5. Kill a tracked NPC. Its `T` and `D` badges, alias, Misc objective, compass marker, and map marker must remain while the body exists, including while it is merely unloaded. If Skyrim later disables, deletes, or otherwise makes the body genuinely unavailable, the objective and alias must clear, the active marker slot must be freed, and an `M` history row with last-known name, identity, and location must remain until Remove Record is used. Save with a dead tracked body, remove its supplying plugin or otherwise reproduce an already-empty alias on load, and confirm the stale objective is hidden and the entry becomes `M` history without touching a newly occupied slot.
6. Save and reload with favorites, recent history, and tracked aliases present. Existing marker objectives must be repaired and visible; an empty Whereabouts state must leave the resident quest's aliases empty, objectives hidden, and notifications silent.
7. Remove a plugin represented by a saved favorite/recent entry. Confirm it says Plugin missing; an unresolved reference from a loaded plugin says NPC unavailable; Remove Unavailable Entries removes only unavailable rows.
8. Track an NPC in another worldspace. Confirm the successful Track status includes a non-blocking notice that its map marker may not appear until that worldspace is entered.
9. On the first tracked NPC, confirm the generic Quest Started banner is suppressed while the NPC objective/name notification, Misc objective, compass marker, and map marker remain.
10. Save and reload with one dead body still marked and one `M` history record. The live corpse marker and missing-body history must retain their distinct states, and the history row must not consume one of the 100 active marker slots.
11. With at least one live NPC tracked, close and reopen Whereabouts several times. There must be no journal-update sound, repeated objective/name notification, or marker churn merely from opening the menu.

## Removal matrix

Use disposable copies of the same save branch.

1. Never use Track, make a new manual save, exit, remove Whereabouts, and load the new save. Record any warnings or symptoms.
2. Track and then untrack every NPC, wait for completion, make a new manual save, exit, remove Whereabouts, and load the new save.
3. Track at least two NPCs, use Settings > Prepare for Uninstall, and confirm commands become blocked while cleanup runs. The completion message must appear only after aliases are empty and the quest is stopped/reset.
4. After successful preparation, confirm Favorites and Recent are empty, make a new manual save, exit Skyrim completely, remove Whereabouts, and load that new save.
   The final session log must contain both `Prepare for Uninstall quest verification passed` and a saved-state verification with `serializable state false`; saving afterward must log that no SKSE serialization records were written.
5. Force or reproduce a cleanup failure if practical. Whereabouts must say not to uninstall and must allow retrying after the failure rather than reporting success.
6. After successful cleanup, open every page. Search, Locations, Tracked NPCs, Favorites, and Recent must show only the prepared-state lock message; Settings must hide ordinary controls and show uninstall instructions plus Resume Whereabouts.
7. Choose Resume Whereabouts. Confirm normal pages return, Favorites/Recent/tracking history remain empty, the Search index refreshes, and no tracking quest starts until an NPC is tracked again.
8. Do not claim that Prepare for Uninstall reverts earlier Travel, Bring, Enable, or Disable actions.

## Virtual Keyboard

Test both Alphabetical and QWERTY layouts. Each character, Space, Backspace, and Clear must update live results when Live search is enabled. Apply must run the expanded submitted search. Cancel must restore the exact text, results, and match total that existed before the keyboard opened.

## SMF visual acceptance

Check the Search, Tracked NPCs, Favorites, Recent, and Settings pages at 1920x1080, 2560x1440, an ultrawide resolution, and with increased font scaling.

- Top controls remain readable, filters/sorting start closed, and the level filter is absent.
- Typing `t` ranks exact and T-prefix names before later substring matches. Search known Reference and Base EditorIDs in mixed case and confirm display-name matches still rank ahead of EditorID-only matches.
- Typing `1a696`, `0001A696`, `0x1a696`, or uppercase equivalents searches exact runtime and plugin-local reference/base FormIDs; typing `Skyrim.esm:01A696` searches stable reference and base identities without another input box. Exact runtime matches rank before local-ID collisions. Hex-like words remain ordinary names unless they contain a digit or use `0x`, and unresolved numeric-looking text falls back to name search.
- With default settings, live search shows at most 50 results and submitted Search, Enter, or Virtual Keyboard Apply shows at most 500, with `shown / matches` at the top. The scroll bar indicates more visible rows and a hard-cap footer asks the user to refine the search. Enable Show all results and confirm both paths return all matches inside the existing scroll area; disable it and confirm the configured limits return.
- Typing `3d` in the plugin filter offers `3DNPC.esp` when that plugin supplies indexed NPCs. Choosing it fills the filter and closes the suggestion list until the field is edited or focused again.
- Generic NPCs are off in ordinary search. Enabling Same location or Followers=Yes turns them on once, a manual override persists inside that active context, and leaving the final automatic context resets them off.
- An unfiltered submitted ordinary-name miss may show at most three conservative `Did you mean` choices. A choice immediately reruns the existing search; Preview searches, FormIDs, stable IDs, active filters, distant terms, and inputs shorter than three folded characters never show suggestions.
- Results and details use a 46/54 side-by-side split at normal widths and stack vertically in narrow content regions. Drag the visible divider to both 30/70 limits in each layout and confirm neither pane becomes unusable; close/reopen the menu and confirm the per-session ratio remains.
- The selected result remains visibly selected after searches and index refreshes.
- After an explicit submitted search, Track, Favorite, Bring, and Enable/Disable refresh the same submitted result set instead of returning to the ten-result Preview.
- Name, plugin/ID, every status badge, location, displayed worldspace, and blank informational-cell space form one continuous row target. Hovering any of them highlights the whole row and clicking any of them selects the same exact actor reference. A row action such as Remove remains independent and never selects the NPC.
- Resize and reorder columns on Search, Tracked NPCs, Favorites, and Recent. The order must remain intact while that table stays alive. Click Search's NPC, Status, and Location headers to toggle ascending/descending sorting. Confirm the advanced Sort controls and header indicator remain consistent. Select Random twice and confirm each explicit selection/search reshuffles while live preview refreshes keep the current order stable.
- Switch List density between Detailed and Compact. Detailed shows identity/worldspace sublines; Compact removes those sublines without losing selection and exposes omitted identity/location context through concise delayed tooltips.
- Search, Tracked, Favorites, Recent, and plugin-suggestion rows use subtle alternating colors that remain readable under every installed SMF theme; hover and selection remain stronger than the stripes.
- Dense rows use delayed colored-letter badges: `A` alive or `D` dead body present, `E` enabled or `X` disabled, `L` loaded or `U` unloaded, plus `T` tracked, `F` follower, `G` generic, and `M` body unavailable when applicable. Full status words remain under the selected NPC name and in NPC Details.
- Every status letter has a distinct semantic color on both light and dark SMF themes; its letter and delayed tooltip remain the non-color meaning cues.
- Long NPC names, locations, plugin identities, and status labels stay clipped within their columns and reveal the full text only after a delayed hover.
- Favorites and Recent rows show enough plugin/RefID and location context to distinguish repeated generic names; selecting a row never triggers Remove.
- Repeating a Recent NPC moves its one row to the top; it never creates duplicates.

## Translation acceptance

1. Test the Main archive with Skyrim's language set to English and at least one non-English language. With only Main installed, every Whereabouts page must remain readable in English; no raw `$Whereabouts_...` key or missing-glyph `?` may appear.
2. Install the matching optional Translation overwrite and repeat. Confirm the translated primary navigation, search controls, filters, settings, and maintenance labels appear while unfilled secondary strings safely remain English.
3. Remove the matching translation file and confirm Whereabouts falls back to its compiled English UI without failing startup or menu registration.
4. Open every page and modal, hover each delayed tooltip, and exercise one success, failure, and confirmation path. Record untranslated fixed text for translator coverage; NPC names, plugin names, locations, FormIDs, and EditorIDs must remain verbatim game data.
- Track creates a visible marker, and Track/Untrack labels refresh after completion.
- The selected NPC name, FormID, location, and plugin remain visible while the command and detail area scrolls.
- The selected header shows explicit Alive/Dead, Enabled/Disabled, and Loaded/Unloaded tags plus Follower, Potential Follower, Tracked, Favorite, and Generic when applicable. `Copy ID` always remains the collapsed label. Selecting FormID, EditorID, or Stable copies immediately and closes the Copy ID popup. EditorID copies the Base NPC EditorID only; unavailable EditorID or Stable values copy FormID and report the fallback. Reference EditorID is not shown in NPC Details.
- Search result status letters include `V` for Favorite. They are display-only: hovering gives the concise meaning, clicking does not change filters or reorder badges, and the Filters and Sorting section remains the only status-filter surface.
- Confirm Search and Tracked labels contain no stray `?` glyphs. Check Detailed and Compact rows, long overflow tooltips, default and custom SMF themes, and the slightly larger initial result pane.
- Refresh Search Index must advance from queued/in progress to a clear completion or failure message and report both NPC and location counts. Prepare for Uninstall must show progress, preserve saved lists on verification failure, and clear tracked state, Favorites, Recent, tracked-death history, and the warning acknowledgement only after verified success.
- On a fresh save, start tracking once and confirm the generic quest-start journal banner is absent while the tracked NPC name/objective notification still appears.

Exercise every `WhereaboutsAPI` version 1 query before readiness, after readiness, across a save-session boundary, and with `None`, a non-actor form, an unknown actor, and a known indexed actor. Failures must be `False` or empty strings and must never rebuild the index or alter the actor, tracking, favorites, recents, or the save.
- Help tooltips do not appear immediately. After a normal mouse-hover delay, only non-obvious controls show help and every disabled command shows its exact unavailable reason.
- Prepare for Uninstall's delayed tooltip and confirmation explain that it clears aliases/objectives and Whereabouts save lists, requires a new manual save and full game exit, and does not undo earlier movement or Enable/Disable commands.
- Confirmation dialogs appear centered in the available SMF viewport.
- Stats, Information, Favor, the standalone About page, and the page-level Refresh buttons are absent.
- The Virtual Keyboard is fully navigable and its Apply and Cancel actions preserve the shared search buffer correctly.
