Scriptname WhereaboutsAPI Hidden

Int Function GetVersion() Global Native
Bool Function SupportsVersion(Int aiVersion) Global Native
Bool Function IsReady() Global Native
Bool Function HasNPC(Form akNPC) Global Native
String Function GetName(Form akNPC) Global Native
String Function GetStableReferenceID(Form akNPC) Global Native
String Function GetReferenceEditorID(Form akNPC) Global Native
String Function GetBaseEditorID(Form akNPC) Global Native
String Function GetLocation(Form akNPC) Global Native
String Function GetCell(Form akNPC) Global Native
String Function GetWorldspace(Form akNPC) Global Native
; 0 = unavailable, 1 = current, 2 = last observed
Int Function GetLocationStatus(Form akNPC) Global Native
Bool Function IsAlive(Form akNPC) Global Native
Bool Function IsEnabled(Form akNPC) Global Native
Bool Function IsLoaded(Form akNPC) Global Native
Bool Function IsFollower(Form akNPC) Global Native
Bool Function IsPotentialFollower(Form akNPC) Global Native
Bool Function IsTracked(Form akNPC) Global Native
Bool Function IsFavorite(Form akNPC) Global Native
Bool Function IsGeneric(Form akNPC) Global Native

; API v2. Unknown or unavailable data returns False, 0, or an empty string.
Bool Function HasTraitData(Form akNPC) Global Native
String Function GetRace(Form akNPC) Global Native
; 0 = unknown, 1 = male, 2 = female
Int Function GetSex(Form akNPC) Global Native
Bool Function HasActorFlagData(Form akNPC) Global Native
Bool Function IsEssential(Form akNPC) Global Native
Bool Function IsProtected(Form akNPC) Global Native
; 0 = unknown, 1 = interior, 2 = exterior
Int Function GetAreaType(Form akNPC) Global Native
; Eight uppercase hexadecimal digits, or empty when no worldspace is known.
String Function GetWorldspaceFormID(Form akNPC) Global Native

Bool Function HasFactionData(Form akNPC) Global Native
Int Function GetFactionCount(Form akNPC) Global Native
String Function GetFactionFormID(Form akNPC, Int aiIndex) Global Native
String Function GetFactionStableID(Form akNPC, Int aiIndex) Global Native
String Function GetFactionName(Form akNPC, Int aiIndex) Global Native
String Function GetFactionEditorID(Form akNPC, Int aiIndex) Global Native

Bool Function HasBaseKeywordData(Form akNPC) Global Native
Int Function GetBaseKeywordCount(Form akNPC) Global Native
String Function GetBaseKeywordFormID(Form akNPC, Int aiIndex) Global Native
String Function GetBaseKeywordStableID(Form akNPC, Int aiIndex) Global Native
String Function GetBaseKeywordName(Form akNPC, Int aiIndex) Global Native
String Function GetBaseKeywordEditorID(Form akNPC, Int aiIndex) Global Native
