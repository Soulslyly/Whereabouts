Scriptname WhereaboutsQuest extends Quest

Bool Function TrackActor(Actor akTarget, Int aiEpoch)
    If akTarget == None || akTarget == Game.GetPlayer() || !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf

    Int aliasID = 2
    While aliasID <= 101
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        ReferenceAlias trackedAlias = GetAlias(aliasID) as ReferenceAlias
        If trackedAlias != None
            ObjectReference currentReference = trackedAlias.GetReference()
            If currentReference == akTarget
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                SetActive(True)
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                SetObjectiveDisplayed(aliasID - 2, True, True)
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                WhereaboutsNative.ReportMarkerState(aliasID - 2, IsActive(), IsObjectiveDisplayed(aliasID - 2))
                Return True
            ElseIf currentReference == None
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                trackedAlias.ForceRefTo(akTarget)
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                SetActive(True)
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                SetObjectiveDisplayed(aliasID - 2, True, True)
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                WhereaboutsNative.ReportMarkerState(aliasID - 2, IsActive(), IsObjectiveDisplayed(aliasID - 2))
                Return True
            EndIf
        EndIf
        aliasID += 1
    EndWhile

    Return False
EndFunction

Bool Function RefreshTrackedActors(Int aiEpoch)
    If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    If !HasTrackedActors(aiEpoch)
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        SetActive(False)
        Return WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
    EndIf

    If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    SetActive(True)
    Int aliasID = 2
    While aliasID <= 101
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        ReferenceAlias trackedAlias = GetAlias(aliasID) as ReferenceAlias
        If trackedAlias != None && trackedAlias.GetReference() != None
            If !IsObjectiveDisplayed(aliasID - 2)
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                SetObjectiveDisplayed(aliasID - 2, True, False)
            EndIf
            WhereaboutsNative.ReportMarkerState(aliasID - 2, IsActive(), IsObjectiveDisplayed(aliasID - 2))
        EndIf
        aliasID += 1
    EndWhile
    Return WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
EndFunction

Bool Function UntrackActor(Actor akTarget, Int aiEpoch)
    If akTarget == None || !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf

    Int aliasID = 2
    While aliasID <= 101
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        ReferenceAlias trackedAlias = GetAlias(aliasID) as ReferenceAlias
        If trackedAlias != None && trackedAlias.GetReference() == akTarget
            If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                Return False
            EndIf
            SetObjectiveDisplayed(aliasID - 2, False, True)
            If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                Return False
            EndIf
            trackedAlias.Clear()
            If !HasTrackedActors(aiEpoch)
                If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                    Return False
                EndIf
                SetActive(False)
            EndIf
            WhereaboutsNative.ReportMarkerState(aliasID - 2, IsActive(), IsObjectiveDisplayed(aliasID - 2))
            Return True
        EndIf
        aliasID += 1
    EndWhile

    Return False
EndFunction

Bool Function ClearTrackedActors(Int aiEpoch)
    Int aliasID = 2
    While aliasID <= 101
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        ReferenceAlias trackedAlias = GetAlias(aliasID) as ReferenceAlias
        If trackedAlias != None
            If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                Return False
            EndIf
            SetObjectiveDisplayed(aliasID - 2, False, True)
            If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
                Return False
            EndIf
            trackedAlias.Clear()
        EndIf
        aliasID += 1
    EndWhile
    If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    SetActive(False)
    Return WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
EndFunction

Bool Function HasTrackedActors(Int aiEpoch)
    Int aliasID = 2
    While aliasID <= 101
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        ReferenceAlias trackedAlias = GetAlias(aliasID) as ReferenceAlias
        If trackedAlias != None && trackedAlias.GetReference() != None
            Return True
        EndIf
        aliasID += 1
    EndWhile
    Return False
EndFunction

Function HandleTrackedDeath(ReferenceAlias akTrackedAlias, Int aiObjectiveIndex, Actor akVictim, Actor akKiller)
    If akTrackedAlias == None || akVictim == None || !akVictim.IsDead() || aiObjectiveIndex < 0 || aiObjectiveIndex > 99 || akTrackedAlias.GetReference() != akVictim
        Return
    EndIf
    If WhereaboutsNative.NotifyTrackedDeath()
        String victimName = ""
        If akVictim != None
            victimName = akVictim.GetDisplayName()
        EndIf

        String killerName = ""
        If akKiller != None
            killerName = akKiller.GetDisplayName()
        EndIf
        String notificationText = WhereaboutsNative.FormatTrackedDeathNotification(victimName, killerName)
        If notificationText != ""
            Debug.Notification(notificationText)
        EndIf
    EndIf

    WhereaboutsNative.ReportTrackedDeath(akVictim, aiObjectiveIndex)
EndFunction

Bool Function RetireTrackedObjective(Int aiObjectiveIndex, ObjectReference akExpectedReference, Int aiEpoch)
    If aiObjectiveIndex < 0 || aiObjectiveIndex > 99 || akExpectedReference == None || !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf

    ReferenceAlias trackedAlias = GetAlias(aiObjectiveIndex + 2) as ReferenceAlias
    If trackedAlias == None || trackedAlias.GetReference() != akExpectedReference
        Return False
    EndIf
    If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    SetObjectiveDisplayed(aiObjectiveIndex, False, True)
    If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch) || trackedAlias.GetReference() != akExpectedReference
        Return False
    EndIf
    trackedAlias.Clear()
    If !HasTrackedActors(aiEpoch)
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        SetActive(False)
    EndIf
    WhereaboutsNative.ReportMarkerState(aiObjectiveIndex, IsActive(), IsObjectiveDisplayed(aiObjectiveIndex))
    Return True
EndFunction

Bool Function RetireEmptyTrackedObjective(Int aiObjectiveIndex, Int aiEpoch)
    If aiObjectiveIndex < 0 || aiObjectiveIndex > 99 || !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf

    ReferenceAlias trackedAlias = GetAlias(aiObjectiveIndex + 2) as ReferenceAlias
    If trackedAlias == None || trackedAlias.GetReference() != None
        Return False
    EndIf
    If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    SetObjectiveDisplayed(aiObjectiveIndex, False, True)
    If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch) || trackedAlias.GetReference() != None
        Return False
    EndIf
    If !HasTrackedActors(aiEpoch)
        If !WhereaboutsNative.IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        SetActive(False)
    EndIf
    WhereaboutsNative.ReportMarkerState(aiObjectiveIndex, IsActive(), IsObjectiveDisplayed(aiObjectiveIndex))
    Return True
EndFunction
