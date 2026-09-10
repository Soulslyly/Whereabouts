# Whereabouts Papyrus API

Read-only NPC information for other Skyrim mods.

Whereabouts 1.0 includes a small, read-only Papyrus API. It lets another mod query information already collected by Whereabouts without changing tracking, Favorites, commands, or save data.

Current API version: `1`

## Adding the API to Your Mod

1. Copy [`WhereaboutsAPI.psc`](../papyrus/Source/WhereaboutsAPI.psc) into your Papyrus source imports for compilation.
2. Call its global functions from ordinary Papyrus code.
3. Do not redistribute `WhereaboutsAPI.pex`; the installed Whereabouts mod supplies the native script.

If your main mod directly calls this API, list Whereabouts as a requirement. If the integration should be optional, the safest layout is a separate compatibility patch that users install only when they also have Whereabouts.

## Quick Example

```papyrus
Function LogWhereaboutsInfo(Actor akActor)
    If akActor == None
        Return
    EndIf

    If !WhereaboutsAPI.IsReady()
        Debug.Trace("Whereabouts API is not ready")
        Return
    EndIf

    If !WhereaboutsAPI.HasNPC(akActor)
        Debug.Trace("Actor is not currently available in Whereabouts")
        Return
    EndIf

    String npcName = WhereaboutsAPI.GetName(akActor)
    String locationName = WhereaboutsAPI.GetLocation(akActor)
    Int locationStatus = WhereaboutsAPI.GetLocationStatus(akActor)

    Debug.Trace("Whereabouts: " + npcName + " | " + locationName + " | status " + locationStatus)
EndFunction
```

## Function Reference

### Availability

#### `Int GetVersion()`
Returns the public API version. Version 1 returns `1`.

#### `Bool IsReady()`
Returns true when Whereabouts has a usable NPC snapshot for the current game session.

#### `Bool HasNPC(Form akNPC)`
Returns true when the supplied actor reference exists in the current Whereabouts snapshot.

### Identity

#### `String GetName(Form akNPC)`
Returns the NPC's displayed name.

#### `String GetStableReferenceID(Form akNPC)`
Returns `Plugin.ext:LocalID` for a stable reference. Returns an empty string for dynamic or otherwise non-persistable references.

#### `String GetReferenceEditorID(Form akNPC)`
Returns the reference EditorID when one exists.

#### `String GetBaseEditorID(Form akNPC)`
Returns the NPC base record's EditorID when one exists.

### Location

#### `String GetLocation(Form akNPC)`
Returns the best available named Location.

#### `String GetCell(Form akNPC)`
Returns the best available cell name.

#### `String GetWorldspace(Form akNPC)`
Returns the best available worldspace name.

#### `Int GetLocationStatus(Form akNPC)`
Returns the source quality of the spatial information:

- `0` — unavailable
- `1` — current
- `2` — last observed

### NPC State

#### `Bool IsAlive(Form akNPC)`
True when the last captured actor state is alive.

#### `Bool IsEnabled(Form akNPC)`
True when the actor is enabled.

#### `Bool IsLoaded(Form akNPC)`
True when the actor is currently loaded.

#### `Bool IsFollower(Form akNPC)`
True when Whereabouts identifies the actor as an active follower.

#### `Bool IsPotentialFollower(Form akNPC)`
True when the actor matches Whereabouts' potential-follower rules.

#### `Bool IsTracked(Form akNPC)`
True when the actor is currently tracked by Whereabouts.

#### `Bool IsFavorite(Form akNPC)`
True when the actor is in the current save's Whereabouts Favorites.

#### `Bool IsGeneric(Form akNPC)`
True when the result represents a generic or non-unique NPC reference.

## Important Behavior

- Pass an actor reference, not an ActorBase. An NPC base form is not a placed actor and will not resolve.
- Call `IsReady()` before querying. During startup, loading, refreshes, or teardown it may temporarily return false.
- Use `HasNPC()` to distinguish an unknown actor from a valid result whose Boolean value happens to be false.
- Invalid forms, non-actor forms, unavailable data, and actors absent from the current snapshot return false, an empty string, or location status `0`.
- Results come from an immutable Whereabouts snapshot. A value can change after the index refreshes.
- Tracked and Favorite state belong to the active save.
- The API is deliberately read-only. It cannot track, teleport, disable, favorite, refresh, or otherwise mutate Whereabouts or an NPC.
- Use normal Papyrus calls. Do not call these native functions from parallel or tasklet contexts.

## Performance Guidance

These calls are intended for occasional UI, quest, dialogue, or compatibility checks. Check readiness once, check the actor once, then read the fields you need. Avoid polling every frame or repeatedly scanning a large actor list from Papyrus.

## Compatibility Guidance

Check `GetVersion()` before depending on behavior added by a future API revision. Version 1 is read-only and does not provide events or subscriptions.

Whereabouts' open permissions allow compatibility patches and integrations. Please credit the original creator and link back to the main Whereabouts page.

## Troubleshooting

### `IsReady()` Stays False

Wait until a save is loaded and Whereabouts has finished building its index. Confirm that Whereabouts and its requirements loaded successfully.

### `HasNPC()` Is False for a Valid NPC

Make sure you passed the placed `Actor` reference rather than `Actor.GetActorBase()`. The NPC may also be outside the current published index until Whereabouts refreshes.

### A String Is Empty

That field is unavailable for the supplied actor. Empty strings are valid fallbacks, especially for dynamic references and unnamed cells or worldspaces.
