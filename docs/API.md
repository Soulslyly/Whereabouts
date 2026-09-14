# Whereabouts read-only Papyrus API

`WhereaboutsAPI.psc` is an optional hidden Global Native API. Version 2 preserves every version 1 function and adds immutable demographic, actor-flag, spatial, faction, and base-keyword observations. It never changes Whereabouts or an NPC.

## Negotiation and readiness

Call `GetVersion()` once and require the version your integration uses. `SupportsVersion(1)` and `SupportsVersion(2)` return true in API v2; other values return false. Existing v1 integrations may keep using `GetVersion() >= 1` and their original functions unchanged.

Call `IsReady()` before querying NPCs. `None`, a non-actor form, an actor absent from the current catalog, a stale save session, or a catalog that is not ready fails closed. String functions return an empty string, integer status functions return `0`, boolean functions return `False`, and facet counts return `0`.

These callbacks are intentionally not tasklet-callable. Call them from ordinary Papyrus event/function execution and cache a group of results instead of polling every scalar getter each frame.

## API v1 — preserved

- `GetVersion`, `IsReady`, and `HasNPC`
- `GetName`, `GetStableReferenceID`, `GetReferenceEditorID`, and `GetBaseEditorID`
- `GetLocation`, `GetCell`, `GetWorldspace`, and `GetLocationStatus`
- `IsAlive`, `IsEnabled`, `IsLoaded`, `IsFollower`, `IsPotentialFollower`, `IsTracked`, `IsFavorite`, and `IsGeneric`

`GetLocationStatus` returns `0` unavailable, `1` current, or `2` last observed.

## API v2

- `HasTraitData`, `GetRace`, and `GetSex` (`0` unknown, `1` male, `2` female)
- `HasActorFlagData`, `IsEssential`, and `IsProtected`
- `GetAreaType` (`0` unknown, `1` interior, `2` exterior)
- `GetWorldspaceFormID`, returned as eight uppercase hexadecimal digits or an empty string
- `HasFactionData`, `GetFactionCount`, and the indexed Faction getters
- `HasBaseKeywordData`, `GetBaseKeywordCount`, and the indexed Base Keyword getters

Facet indices start at `0`. A negative or out-of-range index returns an empty string. Each Faction/Base Keyword item can expose:

- runtime FormID: eight uppercase hexadecimal digits;
- stable ID: `Plugin.ext:localID`, empty for a non-persistable record;
- display name, which falls back to the EditorID and then the eight-digit FormID when the record has no label, so a valid returned facet name is nonempty;
- EditorID, which may be empty when the game does not provide one.

An explicit `Has*Data` function distinguishes known false/empty data from unavailable data. Whereabouts copies all returned data from the currently published immutable catalog; it does not return engine pointers, actor handles, subscriptions, callbacks, or a C++ ABI.
