Scriptname WhereaboutsTrackedAlias extends ReferenceAlias

Int Property ObjectiveIndex Auto

Event OnDeath(Actor akKiller)
    Actor victim = GetActorRef()
    WhereaboutsQuest trackingQuest = GetOwningQuest() as WhereaboutsQuest
    If trackingQuest != None
        trackingQuest.HandleTrackedDeath(Self, ObjectiveIndex, victim, akKiller)
    EndIf
EndEvent
