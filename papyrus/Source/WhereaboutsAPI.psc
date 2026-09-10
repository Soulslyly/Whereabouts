Scriptname WhereaboutsAPI Hidden

Int Function GetVersion() Global Native
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
