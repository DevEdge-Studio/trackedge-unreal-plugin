// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#include "TrackEdgeAsyncStartReplay.h"
#include "TrackEdgeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UTrackEdgeAsyncStartReplay*
UTrackEdgeAsyncStartReplay::StartReplaySession(
	UObject* WorldContextObject
)
{
	UTrackEdgeAsyncStartReplay* Node =
		NewObject<UTrackEdgeAsyncStartReplay>();

	Node->WorldContextObject = WorldContextObject;

	return Node;
}

void UTrackEdgeAsyncStartReplay::Activate()
{
	if (!WorldContextObject)
	{
		OnFail.Broadcast(TEXT("Invalid World"));
		return;
	}

	UGameInstance* GI =
		WorldContextObject->GetWorld()->GetGameInstance();

	if (!GI)
	{
		OnFail.Broadcast(TEXT("No GameInstance"));
		return;
	}

	UTrackEdgeSubsystem* Subsystem =
		GI->GetSubsystem<UTrackEdgeSubsystem>();

	if (!Subsystem)
	{
		OnFail.Broadcast(TEXT("No TrackEdgeSubsystem"));
		return;
	}

	Subsystem->StartReplaySession(
	true,
	[this](bool bSuccess,const FString& Response)
		{
			if (bSuccess)
			{
				OnSuccess.Broadcast(Response);
			}
			else
			{
				OnFail.Broadcast(Response);
			}

			SetReadyToDestroy();
		}
	);
}