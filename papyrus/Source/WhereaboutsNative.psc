Scriptname WhereaboutsNative Hidden

Bool Function IsOperationEpochCurrent(Int aiEpoch) Global Native

Bool Function SetActorEnabled(Actor akActor, Bool abEnabled, Int aiEpoch) Global
    If !akActor || !IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    If abEnabled
        akActor.Enable(False)
    Else
        akActor.Disable(False)
    EndIf
    Return IsOperationEpochCurrent(aiEpoch) && akActor.IsDisabled() != abEnabled
EndFunction

Bool Function OpenActorInventory(Actor akActor, Int aiEpoch) Global
    If !akActor || !IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    akActor.OpenInventory(True)
    Return IsOperationEpochCurrent(aiEpoch)
EndFunction

Bool Function MoveToTarget(Actor akMover, ObjectReference akTarget, Actor akSelectedActor, Bool abEnableSelectedActor, Bool abSettleAfterLoad, Float afSeparation, Int aiEpoch) Global
    If !akMover || !akTarget || !akSelectedActor || !IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf

    If akSelectedActor.IsDisabled()
        If !abEnableSelectedActor
            Return False
        EndIf
        If !IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        akSelectedActor.Enable(False)
        If !IsOperationEpochCurrent(aiEpoch) || akSelectedActor.IsDisabled()
            Return False
        EndIf
    EndIf

    Float heading = akTarget.GetAngleZ()
    Float xOffset = afSeparation * Math.Sin(heading)
    Float yOffset = afSeparation * Math.Cos(heading)
    If !IsOperationEpochCurrent(aiEpoch)
        Return False
    EndIf
    akMover.MoveTo(akTarget, xOffset, yOffset, 0.0, False)
    If abSettleAfterLoad
        Utility.Wait(0.10)
        If !IsOperationEpochCurrent(aiEpoch)
            Return False
        EndIf
        akMover.MoveTo(akTarget, xOffset, yOffset, 0.0, False)
    EndIf
    Return IsOperationEpochCurrent(aiEpoch)
EndFunction

Bool Function NotifyTrackedDeath() Global Native
String Function FormatTrackedDeathNotification(String asVictimName, String asKillerName) Global Native
Bool Function RemoveTrackingOnDeath() Global Native
Function ReportMarkerState(Int aiObjectiveIndex, Bool abQuestActive, Bool abObjectiveDisplayed) Global Native
Function ReportTrackedDeath(Actor akVictim, Int aiObjectiveIndex) Global Native
Function ReportTrackedDeathRemoved(Actor akVictim) Global Native
