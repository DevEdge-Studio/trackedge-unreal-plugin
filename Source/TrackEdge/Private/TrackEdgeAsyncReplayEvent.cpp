// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#include "TrackEdgeAsyncReplayEvent.h"
#include "TrackEdgeSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UTrackEdgeAsyncReplayEvent*
UTrackEdgeAsyncReplayEvent::SetReplayEventType(
	UObject* WorldContextObject,
	ETrackEdgeReplayEventType EventType)
{
	UTrackEdgeAsyncReplayEvent* Node =
		NewObject<UTrackEdgeAsyncReplayEvent>();

	Node->WorldContextObject = WorldContextObject;
	Node->ReplayEventType = EventType;

	return Node;
}

void UTrackEdgeAsyncReplayEvent::Activate()
{
	if (!WorldContextObject)
	{
		OnCompleted.Broadcast();
		return;
	}

	UGameInstance* GI =
		UGameplayStatics::GetGameInstance(WorldContextObject);

	if (!GI)
	{
		OnCompleted.Broadcast();
		return;
	}

	UTrackEdgeSubsystem* Subsystem =
		GI->GetSubsystem<UTrackEdgeSubsystem>();

	if (!Subsystem)
	{
		OnCompleted.Broadcast();
		return;
	}
	
	Subsystem->SetReplayEventType(ReplayEventType);
	OnCompleted.Broadcast();
}