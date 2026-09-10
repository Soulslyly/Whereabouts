# Changelog

## 1.0.1 - Search clarity and recovery

- Keep the Filters and sorting section open when its active-filter count changes.
- Make Alive, Enabled, Follower, Potential Follower, and Loaded filters identify themselves inside their full-width dropdowns.
- Show the number of active filter categories in the collapsed Filters and sorting heading.
- Ignore surrounding whitespace in NPC name, EditorID, FormID, and stable-ID searches.
- Distinguish matches hidden by filters from entries absent from the current index, with explicit clear-and-retry or refresh-and-retry actions.
- Add one concise diagnostic log entry for submitted zero-result searches to make missing-NPC reports actionable.

No ESP, Papyrus, tracking, command, serialization, save schema, settings schema, dependency, or runtime-support changes are included.

## 1.0.0 - Release candidate

- Restore the Whereabouts journal title and description. Preserve NPC objective notices while marking the quest's introductory HUD announcement handled.
- Correct stopped-quest detection and verify tracking readiness before Resume unlocks commands. Failed restart no longer presents successful uninstall instructions.
- Verify active map objectives before reporting successful tracking.
- Keep saved NPC histories distinct across reused runtime IDs and enforce the favorite limit before a save record can overflow.
- Make unavailable-entry cleanup conservative, align Favorites cleanup controls, wrap plugin filters, and refresh search limits without changing search mode.
- Correct failed-command state rollback, session settings after a disk-write failure, and UTF-8 Backspace.
- Preserve translator edits on regeneration and correct safety/recovery instructions in all eight optional language files.
- Include standalone SMF validation in the independently rebuildable Source package.

Final-hash Skyrim acceptance remains required, including initial banner/sound suppression and Prepare for Uninstall > Resume > Track.

## 1.0.0-beta.34 - Runtime-test corrections

- Report an inactive `Whereabouts.esp` explicitly instead of the ambiguous tracking-quest-unavailable error. The observed 1.5.97 failure was caused by the ESP not being enabled; the DLL, SMF, BEES, index, and search had initialized successfully.
- Open the Virtual Keyboard through the root modal scope after its Search-page button requests it.
- Support multiple case-insensitive exact plugin filters with OR behavior across NPC and location results.
- Add a default-off Show all results setting for live and submitted searches while retaining the scrollable result panes.
- Remove the tracking quest display name and startup log so its first start is silent without changing the 100 aliases, objectives, VMAD, FormIDs, or save schema.

Tracking on 1.5.97 still requires `Whereabouts.esp` to be enabled and remains subject to in-game confirmation with this exact archive.

## 1.0.0-beta.33 - Final compatibility candidate

- Add Skyrim 1.5.97 / SKSE 2.0.20 to the universal DLL compatibility declaration while retaining the Address Library format-2 and format-5 paths used by newer runtimes.
- Validate all four Papyrus scripts against the exact local 1.5.97 import set as well as the existing 1.6.1170 and 1.7.104 sets.
- Show the stable release number in About and replace the overly detailed Where Are You wording with a short inspiration credit.
- Route tracked-NPC death notifications through the selected Whereabouts translation and complete a final player-facing localization, package, dependency, and save-safety audit.

No save schema, INI schema, ESP record, VMAD, alias, objective, FormID, command, tracking, or menu layout changes are intended. In-game acceptance remains required on every advertised runtime.

## 1.0.0-beta.32 - Language selection and translation cleanup

- Added a global restart-required language selector with `Follow Skyrim` as the default.
- Added current-language feedback and safe English fallback without changing Skyrim's own language.
- Cleaned the canonical English translation resource and added public translator instructions.
- Corrected the non-ASCII translation generator for Windows PowerShell 5.1.
- Corrected the public dependency guide so it no longer claims PowerShell 7 is required.
- Preserved save data, ESP, Papyrus, commands, tracking, dependencies, and runtime targets.

## 1.0.0-beta.31 - Search and UI completion

- Raise the default live/submitted result limits to 50/500, expose global 10-100 and 50-1000 controls, and retain a hard 1000-row safety ceiling.
- Replace Include locations with explicit NPCs only, NPCs and locations, and Locations only search modes; mixed results can place either section first.
- Rename NPC sorting to NPC location, compact the filter panel, wrap the Search toolbar responsively, and make location rows honor Detailed/Compact presentation on both pages.
- Align Location details and add an immediate stateless Copy menu for FormID, EditorID, Stable identity, and a COC command.
- Prevent mixed-search typo suggestions when either the NPC or location catalog already matched.
- Expand the translation catalog and all English fallback/starter resources to 333 matching keys.
- Preserves beta.30 save data, plugin records, Papyrus behavior, command lifecycle, dependencies, and runtime targets.

## 1.0.0-beta.30 - Runtime foundations

- Make translation loading independent of Scaleform translator timing and retry safely after transient resource failures.
- Dispatch location travel once after SKSE Menu Framework closes, with session cancellation, target revalidation, and diagnostic logging.
- Start the tracking quest only for first-track or tracked-save repair and log every legitimate start attempt.
- Remove unsafe target-height movement settling and reject stale or identity-mismatched dynamic actor targets.
- Replace the prerelease display-string location status API with stable numeric status values.
- Lock the UI after verified Prepare for Uninstall and provide an explicit empty-state Resume action.

No save schema, INI schema, ESP, VMAD, alias, objective, FormID, master, or runtime target changed. In-game acceptance remains required.

## 1.0.0-beta.29 - Audit corrections

- Public Source packaging excludes Whereabouts developer tests and CommonLib test fixtures while retaining a clean production rebuild path.
- Exact runtime location FormIDs rank before plugin-local-ID collisions.
- Locations controls and stacked panes adapt to narrow and short SMF windows.
- Clipped location identity and context fields expose delayed full-text tooltips in both Search and Locations.
- Combined-catalog status and diagnostics consistently use Search index wording.

No save, INI, Papyrus, ESP, VMAD, command, tracking, dependency, or runtime-target schema changed. In-game acceptance remains required.

## 1.0.0-beta.28 - Location search and travel

- Add a dedicated Locations page backed by a transient, immutable catalog of stable cell records.
- Search cell names, EditorIDs, runtime/local/stable FormIDs, source plugins, containing Location labels, and worldspaces with 10-row live previews and 128-row submitted searches.
- Add a default-off Include locations filter to the main Search page with separate NPC/Location tables and one combined result limit.
- Show cell identity, containing Location, worldspace, interior/exterior type, and exterior grid details.
- Add confirmation-only location travel that closes SKSE Menu Framework, re-resolves and identity-checks the cell on the game task, and uses CommonLibSSE-NG's typed cell-loading function.
- Balance mixed NPC/location result limits so one result kind cannot hide the other, and skip cell-catalog searches entirely while Include locations is off.
- Prefer readable named parent Locations over internal child EditorIDs and rename the shared maintenance action to Refresh Search Index with both catalog counts.
- Expand the 311-key localization catalog and optional starter resources without adding save data, settings, Papyrus behavior, quest state, or ESP records.

Debug/Release, dual Papyrus, typed ESP, native metadata, and translation validation pass. Location behavior remains untested until the exact packaged beta.28 candidate is exercised in both target runtimes.

## 1.0.0-beta.27 - UI/search cleanup and uninstall verification

- Make list status letters display-only and remove their misleading click-to-filter help; status filtering remains in Filters and Sorting.
- Make the Search table's Status header sortable, add transient Random order, and keep Random previews stable until the next explicit submitted search.
- Place Plugin and Location filters together on wide layouts, remove the redundant text-filter heading, widen aligned details labels, and remove Reference EditorID from NPC Details.
- Define Copy ID's EditorID choice as Base NPC EditorID only.
- Repair Prepare for Uninstall verification by checking the meaningful stopped/empty/hidden quest invariants instead of CommonLib's independent running predicate, which can remain true for a stopped quest.
- Harden translation newline validation, regenerate 290-key English fallbacks and optional starter translations, correct active release documentation, and document the dynamic Visual C++ runtime requirement.

Static build and package validation passes; player-visible behavior remains untested until the packaged beta.27 candidate is exercised in Skyrim.

## 1.0.0-beta.26 - Audit and release-gate corrections

- Complete the SKSE Menu Framework preflight for all 79 exports used by Whereabouts and require an exact installed SMF binary for owner-side final archive validation.
- Localize fixed player-facing status text and formatted UI messages through a safe translated-format path that falls back to readable English when a translation has malformed placeholders.
- Expand the English fallback and eight optional starter translation files from 242 to 292 matching keys.
- Correct third-party notice wording, release-version metadata, and validation claims while keeping public Source builds portable.
- Keep save data, uninstall ordering, commands, tracking, public API, Papyrus behavior, and plugin records unchanged.

Player-visible behavior remains untested until the packaged beta.26 candidate is exercised in Skyrim.

## 1.0.0-beta.25 - Release foundation corrections

- Preserve beta.24 unchanged and open the review-correction snapshot.
- Correct composable status-filter toggling, remove unsupported HUD notification-queue mutation, and retain the ESP-owned tracking notification contract.
- Establish a 242-entry standard SKSE translation catalog, English fallback resources for every supported file name, a separate eight-language starter archive, reproducible .NET 9 tooling, complete redistributable notices, and inspectable DLL version metadata.
- Keep save data, uninstall ordering, commands, tracking capacity, public API, plugin records, and deployed files unchanged.

Player-visible behavior remains untested until the packaged beta.25 candidate is exercised in Skyrim.

## 1.0.0-beta.24 - UI and maintenance correction

- Preserve beta.23 unchanged and open the approved pre-localization UI and maintenance phase.
- Restore Copy ID as one immediate FormID/EditorID/Stable action menu with explicit FormID fallback.
- Add the `V` Favorite badge, clickable transient status filters, active-badge priority, theme-derived full-tag colors, and a slightly larger default result pane.
- Reorganize Settings and Filters, align detail values, remove redundant spatial rows, compact Recent maintenance actions, and replace font-unsafe punctuation.
- Give index refresh and Prepare for Uninstall independent progress/success/failure feedback while preserving fail-closed cleanup ordering.
- Clear save-specific command and maintenance messages at session boundaries, and stop prioritizing a clicked status badge after its matching filter is manually changed.
- Remove only the tracking quest's generic startup journal entry; keep the quest, 101 aliases, 100 objectives, VMAD bindings, and per-NPC objective notifications.
- Keep persistence, dependencies, public API, tracking capacity, and the HEDR 1.71 allocation contract unchanged.

Player-visible behavior remains untested until the packaged beta.24 candidate is exercised in Skyrim.

## 1.0.0-beta.23 - Feature completion and public read API

- Preserve beta.22 unchanged and open one consolidated pre-1.0 feature-completion phase.
- Repair the Copy ID interaction, expose independent console/crosshair auto-selection settings, add conservative potential-follower discovery, and clarify/expand location presentation.
- Improve selected-row visibility and compact list actions without adding another theme or persistent layout file.
- Add a versioned, read-only Papyrus API over Whereabouts' immutable published NPC snapshot; no third-party mutation API, hook, save record, or new dependency is introduced.

Player-visible behavior and third-party Papyrus calls require Skyrim testing before this candidate can be promoted.

## 1.0.0-beta.22 - Final pre-release corrections

- Preserve beta.21 unchanged and open a narrowly scoped final correction phase.
- Repair the tracked-death recovery path when Skyrim has already cleared the dead actor's tracking alias.
- Align public lifecycle, SMF compatibility, console retry, and empty-save documentation with the implemented behavior.
- Rebuild and revalidate clean runtime and optional Source archives without copied generated output.

The ESPFE layout, INI schema, SKSE serialization schema, tracking capacity, command set, and dependencies are unchanged. Static validation passes; player-visible behavior still requires testing in Skyrim.

## 1.0.0-beta.21 - Runtime safety and identity corrections

- Preserve beta.20 unchanged and open a focused movement, EditorID, compatibility-lifecycle, and uninstall-observability phase.
- Treat both reported Dimhollow crashes as unresolved runtime evidence; do not claim relocation is fixed until the final beta.21 archive passes Skyrim testing.
- Resolve reference and base EditorIDs from Skyrim's guarded editor-ID registry while preserving known IDs during targeted refreshes.
- Replace the fixed-attempt console-selection cutoff with a two-second deadline, remove the stale SMF float-version probe, and make SMF page registration an irreversible fail-closed once gate.
- Require explicit enabling before relocating disabled NPCs; cross-boundary Travel performs a guarded post-load settle pass while Bring remains single-pass.
- Write no SKSE serialization records when Whereabouts has no durable state, and log exact quest plus saved-state cleanup verification.
- Preserve the ESP, tracking quest layout, INI schema, cosave schema version, and deployed files.

## 1.0.0-beta.20 - Compatibility foundation

- Replace the semantically invalid SKSE Menu Framework float-version comparison with loaded-module, fixed-file-version, and required-export validation.
- Make optional More Informative Console selection refresh use frame-separated bounded attempts and validate its actual Scaleform destination before invoking it.
- Cancel pending console work on close and lifecycle boundaries, and keep unsupported framework environments fail-closed.
- Add an evidence-qualified compatibility matrix without changing the ESP, Papyrus, VMAD, INI, or cosave schema.

The ESPFE, Papyrus scripts, VMAD, INI, cosave schema, search behavior, tracking behavior, and command set are unchanged. Player-visible behavior remains untested in Skyrim.

## 1.0.0-beta.19 - Search and compact-view corrections

- Treat typo distance and length limits as Unicode code points rather than UTF-8 bytes.
- Keep typo suggestions aligned with default search by excluding hidden generic NPCs and suppressing suggestions whenever any search filter, including Include Generic NPCs, is active.
- Keep missing-body tracked records to one visible line in Compact density while retaining identity in a delayed tooltip.
- Keep SKSE Menu Framework 3.14 as the verified minimum; the older 3.8 runtime remains unsupported until its ABI is independently proven.

The ESPFE, Papyrus scripts, cosave schema, tracking capacity, commands, hooks, and dependencies are unchanged. Player-visible behavior requires testing in Skyrim.

## 1.0.0-beta.18 - UI and search completion

- Add optional reference/base EditorID capture, case-insensitive EditorID search in the existing field, and EditorIDs in selected-NPC details.
- Replace the global Copy ID preference with a per-use dropdown for Runtime RefID, stable Plugin:Local RefID, Reference EditorID, or Base EditorID; unavailable identities stay disabled.
- Add conservative clickable typo suggestions only after explicit, unfiltered, zero-result name searches.
- Make Search, Tracked, Recent, and Favorites rows one continuous selection surface across all informational cells and blank space, with one theme-derived row highlight and independent action buttons.
- Add Detailed and Compact list density, resizable/reorderable columns, Name/Location header sorting, and a clamped per-session results/details splitter.
- Preserve the active Submitted or Preview search mode across command and index refreshes.

The ESPFE, Papyrus scripts, cosave schema, tracking capacity, commands, hooks, and dependencies are unchanged. Player-visible behavior requires testing in Skyrim.

## 1.0.0-beta.17 - Search refresh and theme contrast correction

- Preserve the active Submitted or Preview search mode when an index revision refreshes the current result set.
- Replace source-text-only search-refresh coverage with a behavioral state-transition regression.
- Adjust every status-letter color against the effective SMF child/window and current table-row background instead of relying only on a light/dark palette choice.
- Add contrast and row-compositing tests for dark, light, middle-luminance, and colored theme backgrounds while retaining distinct status meanings and text/tooltips as non-color cues.

The ESPFE, Papyrus scripts, save/settings schemas, dependencies, commands, tracking behavior, FormID matching, and worldspace presentation remain unchanged. Player-visible behavior requires testing in Skyrim.

## 1.0.0-beta.16 - Search and UI checkpoint

- Preserve Preview or Submitted mode when commands and targeted index updates refresh Search; explicit edits still intentionally return to the ten-result live preview.
- Expand the existing search field so a plain numeric FormID matches exact runtime reference/base IDs and plugin-local reference/base IDs without requiring a plugin name. Exact runtime matches rank first and ambiguous local IDs remain visible for plugin-based disambiguation.
- Keep hex-like words such as `dead` and `decade` as name searches; accept all-letter hexadecimal IDs through an explicit `0x` prefix.
- Add exterior parent/save-cell worldspace fallback, include worldspace in Location filtering, and show non-duplicate worldspace beneath Search result locations.
- Make result names, identities, locations, and displayed worldspaces select the same exact actor reference.
- Remove the redundant `More results below` banner; the scroll bar communicates additional rows and the existing Show More action appears at the end of a truncated Preview.
- Give every dense status letter a unique semantic color selected from light/dark palettes after classifying the active theme's blended background.
- Add focused parser, engine, presentation, refresh, footer, palette, and headless SMF source-contract regressions.
- Repair the previously unregistered Papyrus pipeline validator for current epoch-aware signatures, isolate its fake compiler output in a temporary fixture, and register it in both build configurations.

The ESPFE, Papyrus scripts, save/settings schemas, dependencies, tracking state, and command implementation are unchanged. Static validation passes; player-visible behavior remains unconfirmed until the exact beta.16 archives are tested in Skyrim.

## 1.0.0-beta.15 - Command checkpoint test candidate

- Activate the initial command operation session at Data Loaded so the Search index and actor commands no longer start with independent readiness states.
- Preserve save/load/revert ownership: initial activation can open epoch 1 or observe an already-active session, but cannot reopen an advanced suspended boundary.
- Keep menu registration available if Data Loaded catches a newer session during a transient boundary; commands remain gated until that boundary resumes normally.
- Add explicit startup, lifecycle-resume, command-rejection, and submission-failure diagnostics.
- Add RED/GREEN lifecycle and plugin-registration regressions for the beta.13/beta.14 all-command no-op failure.
- Preserve the ESP, Papyrus, cosave/settings schemas, dependencies, command semantics, and current UI unchanged.

Runtime behavior remains unconfirmed until this exact beta.15 command checkpoint is tested in Skyrim. Submitted-search persistence, raw FormID runtime reproduction, worldspace metadata, and the remaining UI foundation are intentionally not included in this checkpoint.

## 1.0.0-beta.14 - Unreleased

- Carry one captured operation epoch from every menu entry through task submission instead of recapturing at the queue boundary.
- Carry both operation epoch and request serial through Enable/Disable completion and every verification retry, rejecting prior-session and three-toggle ABA results.
- Serialize completion publication against save/session advancement and remove the redundant second tracking-completion queue.
- Restore optimistic Enable/Disable state when initial SKSE task submission fails, without reading game state from the UI thread.
- Add deterministic boundary, concurrency, whole-snapshot, request-order, and source-wiring regressions.
- Replace named internal-tool exclusions in the public Source validator with a tool-neutral hidden-directory rule.
- Preserve the beta.13 ESP, Papyrus, cosave, settings, dependencies, UI features, and commands unchanged. Skyrim behavior remains an in-game acceptance gate.

## 1.0.0-beta.13 - Rejected before runtime testing

- Add one token-bound transient operation epoch across queued UI work, commands, tracking, Papyrus completion, save transitions, and Prepare for Uninstall.
- Make serialization boundaries invalidate stale work without waiting for an in-progress NPC catalog capture, and prevent stale ready handlers from resuming a newer save session.
- Save and load Favorites, Recent, tracked-death history, and the tracking warning as one coherent snapshot; a failed load cannot expose prior-save or partially decoded data.
- Publish immutable runtime settings and use one fixed completion mailbox plus narrow request serials to reject reordered Enable/Disable and target-selection results.
- Resolve the 100 marker slots by exact alias ID and objective index, serialize marker mutations through one ledger, and retain exact reference-handle ownership for body retirement.
- Guard Papyrus actor and quest mutations with the originating operation epoch while keeping movement, inventory, and Enable/Disable inside the established Papyrus bridge.
- Make Prepare for Uninstall advance ownership first, stop/reset the quest with bounded next-frame verification, preserve data on failure, and clear data only after exact cleanup success.
- Consolidate callbacks under one process-lifetime AppContext, contain callback exceptions, and keep partially initialized capabilities resident but unavailable after irreversible registration.
- Preserve the beta.12 ESP bytes, public settings and cosave formats, dependency set, and player-facing feature set.
- Pass 172 Debug and 172 Release tests plus exact dual-runtime Papyrus compilation. Skyrim behavior remains an in-game acceptance gate.

## 1.0.0-beta.12 - Unreleased

- Advance the transient index session token before waiting for an in-flight catalog capture, so readers reject the old catalog immediately and the stale capture cannot publish data or failure into the new session.
- Serialize request observation, reservation, metadata publication, task submission, and save-boundary invalidation under one coordinator boundary.
- Exercise the shipping RuntimeIndex and IndexCoordinator implementations directly with deterministic capture, scheduling, tracked-ID, and marker-repair boundaries.
- Add focused regressions for stale success/failure rejection, enqueue ordering, first-open coalescing, fixed scheduler/capture failures, retry behavior, and targeted no-op publication.
- Record full-build duration, targeted catalog clones, and suppressed no-op publications in debug diagnostics without changing save data or player-facing behavior.
- Preserve the beta.11 ESP, Papyrus, configuration, save schema, UI, and gameplay surface.

## 1.0.0-beta.11 - Rejected before runtime testing

- Add one canonical placed-reference/base identity model so generic actors sharing a base remain distinct and persistence always uses the stable reference identity.
- Publish search data as coherent immutable session/revision views with one serialized full-rebuild owner, no-op suppression, and readiness/failure preservation during targeted updates.
- Initialize the NPC catalog on the first menu open without requiring console/crosshair selection, coalesce repeated requests, and keep marker repair out of first-open work.
- Invalidate the transient catalog at SKSE Load/Revert boundaries before save records are processed so prior-save views and stale builds cannot become current.
- Centralize invariant Unicode lowercase matching for names, plugins, locations, suggestions, and stable identities, with a safe byte-preserving fallback for malformed UTF-8.
- Separate Location, Cell, Worldspace, source, and freshness presentation; keep unavailable targets honest by labeling retained copied values as last observed.
- Show preparing/fixed-failure states without a misleading empty-result summary and keep same-session retained results usable after a failed refresh.
- Harden Source packaging against internal project state, arbitrary source extensions, stale payloads, and concurrent validation while preserving the exact ten-file Main contract.

## 1.0.0-beta.10 - Unreleased

- Stop reopening Whereabouts from replaying tracked objective notifications.
- Add a default-off setting for automatically selecting the console-targeted NPC when Whereabouts opens.
- Refresh More Informative Console's lower-right data through its optional detected interface when a safe supported path is available.

## 1.0.0-beta.9 - Unreleased

- Centralize numeric and player-facing versions in `version.json` and make build, validation, packaging, DLL, and menu surfaces consume or validate that contract.
- Rebuild `Whereabouts.esp` independently from explicit typed quest, startup-stage, alias, objective, and VMAD records; the source package no longer requires an original/template plugin.
- Remove Regex Search from the search engine, settings UI, saved INI output, and default configuration while silently tolerating the retired INI key.
- Keep Bring and Enable/Disable inside Whereabouts under the default inline-command behavior; Travel, Inventory, and Select in Console still close or replace the menu as required.
- Preserve the working 100-marker quest identity and leave disabled tracked-NPC marker handling unchanged.

## 1.0.0-beta.8 - Unreleased

- Improve large-result navigation, theme-aware list readability, clipped-text discovery, compact colored letters for every core status, and plugin suggestion behavior.
- Refresh More Informative Console only after its optional interface reports ready while retaining the vanilla console path when it is absent.
- Keep map markers on dead tracked NPCs and retain a removable missing-body history record only after Skyrim makes the body demonstrably unavailable.
- Shorten ordinary help tooltips and preserve exact explanations for unavailable commands.
- Reconcile dead-body cleanup even after the final active alias disappears and prefer stable plugin/local identity over a reused runtime FormID.
- Run Enable/Disable through a completion-checked Papyrus call, close the paused menu, retry across its closing frame, and verify the resulting live actor state after gameplay resumes.
- Compile all three scripts against the exact 1.7.104 Creation Kit and SKSE 2.3.1 sources with zero errors and warnings; confirm the new runtime foundation boots before candidate installation.

## 1.0.0-beta.7 - Unreleased

- Preserve the user-confirmed beta.6 tracking marker lifecycle while addressing the remaining Enable/Disable, console-overlay, search, notification, and usability work.
- Add pause-safe bidirectional Enable/Disable feedback, feature-detected More Informative Console repainting, and exact suppression of only Whereabouts' initial Quest Started banner.
- Add configurable Copy ID output, forgiving runtime FormID input, clearly labeled contextual generic NPC discovery, and a different-worldspace marker notice.
- Expand Prepare for Uninstall guidance. Where Are You by k0mp1ex was used as inspiration; its credit remains, and no code or assets from it are distributed.

## 1.0.0-beta.6 - Unreleased

- Restore the original working quest's Start Game Enabled lifecycle while keeping Run Once clear so beta saves previously stopped by beta.5 can restart safely.
- Keep the tracking quest resident during ordinary refresh, untrack, clear-all, and death-removal operations instead of stopping and resetting it whenever the final alias clears.
- Reserve quest stop/reset for the explicit Prepare for Uninstall workflow.
- Add regressions for the persistent marker lifecycle after beta.5 runtime logs showed successful alias fills but rejected objective display state.

## 1.0.0-beta.5 - Unreleased

- Repair tracking quest restartability so markers can be displayed after an empty quest was stopped and reset.
- Settle Enable/Disable state through bounded live refreshes instead of assuming the first post-command frame is authoritative.
- Keep Clear Selection cleared while the same console target remains selected, and republish Select in Console after the console is ready.
- Add delayed, restrained help tooltips, exact unavailable-command reasons, compact NPC status tags, and Copy ID.
- Preserve finished staging trees and release ZIPs when the archive-tamper regression runs, including concurrent Debug/Release validation.

## 1.0.0-beta.4 - Unreleased

- Preserved beta.3 as the ship-validated movement-correction candidate and opened a non-gameplay validation-hardening snapshot.
- Excluded copied build, staging, release, and PluginBuilder cache output from the new source snapshot.
- Began work on concurrency-safe archive validation and the exact-evidence gate required before Skyrim 1.7.104 support can be advertised.
- Added a two-process archive regression and serialized the destructive archive test with a bounded named mutex; the unlocked source reproduced the collision and the corrected source passes.
- Verified that the local Steam executable and SMF package are ready for the 1.7.104 phase, while exact SKSE 2.3.1 and Address Library 1.7.104 inputs remain missing and are not substituted with nearby versions.
- Rebuilt Debug and Release from clean CMake trees, passed 85/85 tests in each configuration, compiled all three Papyrus scripts with zero errors/warnings, and regenerated/validated the ESPFE with typed and dedicated header tooling.

## 1.0.0-beta.3 - Unreleased

- Preserved beta.2 after a second audit found caller-side deletion of Papyrus dispatch arguments on the new Travel/Bring path.
- Opened a narrow correction snapshot to align movement dispatch ownership with the pinned CommonLib and existing project call pattern; beta.2 must not be installed.
- Removed the caller-side delete so the Papyrus VM retains ownership of the dispatched movement arguments.
- Added a regression that rejects caller-side deletion after Papyrus dispatch; the copied beta.2 source fails it and beta.3 passes it.
- Rebuilt Debug and Release, passed 84/84 tests in each configuration, compiled all three PEX files with the official compiler at zero errors/warnings, and revalidated the unchanged ESPFE.
- Serana Bring remains an in-game acceptance gate; static validation does not prove that the original crash is resolved.

## 1.0.0-beta.2 - Unreleased

- Opened a crash-isolation build for the user-reported Serana Bring crash.
- Replaced the synchronous native movement sequence used by Travel and Bring with one latent Papyrus `MoveTo` call that applies the configured separation as part of the relocation.
- Removed the immediate raw `SetPosition` adjustment that could overlap unloaded actor 3D attachment and render setup.
- Added request and completion logging for Travel and Bring, including the selected, moving, and destination FormIDs.
- Added a token-based in-flight guard so overlapping movement requests are rejected and stale Papyrus completions cannot unlock a newer request.
- Moved the optional Enable step into the same guarded Papyrus operation so a rejected overlapping request cannot enable or attach an actor first.
- Added regression coverage requiring both movement commands to remain asynchronous across the Papyrus bridge.

## 1.0.0-beta.1 - Unreleased test candidate

- Reset the public version line to the first `1.0.0` beta while preserving the earlier local development snapshots as history.
- Reworked command confirmations as centered root-level SMF modals so Disable and other guarded actions can actually receive input.
- Made Track, Untrack, Enable, and Disable refresh the selected NPC from exact command completion instead of requiring the menu to be reopened.
- Hardened the on-demand tracking quest: bounded script-readiness checks, forced objective redisplay, automatic stop/reset after the final alias clears, and matching cleanup when death removal clears the final alias.
- Added a once-per-save warning before the first Track and a verified Prepare for Uninstall workflow.
- Added meters, feet, and game-unit distance displays, with meters as the default.
- Added runtime FormID and `Plugin.ext:localID` matching to the existing search field, plus a Same location filter.
- Distinguished missing plugins from unavailable NPC references in Favorites and Recent and added bulk removal of unavailable entries.
- Renamed player-visible controller search input to Virtual Keyboard.
- Changed Select in Console to issue a normal sanitized `prid` command first, with a verified direct fallback, so console extensions can observe the standard selection path.
- Removed the obsolete Enable Whereabouts setting while retaining legacy-key tolerance during INI loading.
- Expanded automated coverage to 83 Release tests. Quest-marker visibility and third-party console-overlay repainting remain in-game acceptance gates.

## 1.2.0 - Unreleased

- Changed teleport confirmation to a risk-based default: unloaded NPCs in the same exterior or interior area can be moved without a prompt, while different cells, different worldspaces, unknown location relationships, disabled actors, dead actors, and protected Bring actions retain appropriate safeguards.
- Refreshed the selected NPC's live state when the framework opens and after synchronous commands so Enable/Disable labels and movement decisions do not remain stale.
- Repaired tracking objectives by activating the quest before objective display, forcing redisplay for occupied aliases, and refreshing existing markers after load or manual index refresh.
- Made Recent a unique most-recent-first history: selecting an existing NPC moves it to the top instead of creating another row.
- Added immediate controller-keyboard previews, exact Cancel restoration, and selectable Alphabetical or QWERTY layouts.
- Published console selection through the game's ConsoleData update path so console-extension overlays can observe the selected reference, with a direct fallback if message infrastructure is unavailable.
- Removed all Whereabouts-specific gameplay keybind code and configuration.
- Improved SMF-native readability with centered confirmations, clearer spacing and section titles, full-width two-column commands, grouped details, and a selected-NPC identity header above the scrolling details area.

## 1.1.0 - Unreleased

- Ranked normal name searches by exact, prefix, word-prefix, and substring relevance.
- Kept live results compact at 10 and expanded submitted searches to as many as 128, with an accurate total-match count.
- Changed regex matching to partial-name search and moved the mode toggle to Settings.
- Added live plugin-name suggestions and removed the level filter and result-limit control from Search.
- Renamed the current teammate filter and detail field to Follower for clarity; this still means currently following.
- Collapsed filters, advanced settings, About information, and extended NPC details by default.
- Put commands directly below the compact selected-NPC summary and removed Stats, Information, and Favor commands.
- Rebuilt Favorites and Recent rows as two-column tables so Select and Remove no longer overlap or clip.
- Moved index refresh to Settings and removed the standalone About page.
- Refreshed selected NPC state after command completion so Track/Untrack and Enable/Disable labels stay current.
- Activated the tracking quest when a marker is added and deactivated it after the final marker is removed.
- Added the SKSE Menu Framework API notice and limited the optional CommonLib source to exact build and compatibility-test inputs. A temporary reference-only Where Are You notice was later removed after confirming no code or assets were distributed.

## 1.0.2 - Unreleased

- Migrated the native build to maintained CommonLibSSE-NG v7.2.0 with Address Library database-format 2 and 5 support.
- Declared Address Library v5 compatibility while retaining the Skyrim 1.6.1170 / SKSE 2.2.8 minimum.
- Added regression coverage for both Address Library formats and the exported SKSE plugin-version declaration.
- Verified the SKSE Menu Framework 3.14.1 consumer header against the official API source and audited its Skyrim 1.7 compatibility build.
- Kept CommonLibSSE-NG source and legal notices in the optional source package while adding the required license and Modding Exception to the main package.
- Built and passed all 48 native tests in Debug and Release; in-game testing remains required before either runtime is advertised as confirmed.

## 1.0.1 - Unreleased

- Corrected SMF custom-window rendering, tracking refresh timing, plugin filtering, and settings persistence.
- Corrected Papyrus output staging, quest identity/startup policy, Release path hygiene, and final ZIP content validation.
- Ensured Papyrus-backed actions start their bridge quest and disabled-actor movement waits for Enable to finish.
- Compiled all three scripts with the official Creation Kit compiler and matching Skyrim/SKSE sources with zero errors or warnings.
- Produced a clean six-file 1.0.1 runtime test candidate; runtime support remains unconfirmed until the in-game acceptance matrix passes.

## 1.0.0 - Unreleased

- Initial release.
- Added SKSE Menu Framework search, filters, target capture, NPC details, commands, favorites, recent history, and tracked-NPC pages.
- Added 100 quest-alias map markers with optional death notification and marker removal.
- Added guarded teleport, inventory, favor, enable/disable, and console-selection actions.
- Added per-save favorites/recent serialization and global INI settings.
- Added visible selection-source labels, Clear Selection, detailed race/sex/cell/worldspace information, settings reset, and save-transition selection cleanup.
- Corrected configurable death handling so retaining a dead tracked NPC also retains its map objective.
- Removed UIExtensions, MCM Helper, SkyUI MCM, and PapyrusUtil as direct requirements.
