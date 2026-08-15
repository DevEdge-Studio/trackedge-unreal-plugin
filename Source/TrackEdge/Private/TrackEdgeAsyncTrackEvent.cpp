// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#include "TrackEdgeAsyncTrackEvent.h"
#include "TrackEdgeSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UTrackEdgeAsyncTrackEvent* UTrackEdgeAsyncTrackEvent::TrackEventAsync(
	UObject* WorldContextObject,
	const FString& EventName
)
{
	UTrackEdgeAsyncTrackEvent* Node = NewObject<UTrackEdgeAsyncTrackEvent>();
	Node->WorldContextObject = WorldContextObject;
	Node->EventName = EventName;
	return Node;
}

void UTrackEdgeAsyncTrackEvent::Activate()
{
	if (!WorldContextObject)
	{
		OnFail.Broadcast(TEXT("Invalid World Context"));
		SetReadyToDestroy();
		return;
	}

	UGameInstance* GI =
	UGameplayStatics::GetGameInstance(WorldContextObject);

	if (!GI)
	{
		OnFail.Broadcast(TEXT("No Game Instance"));
		SetReadyToDestroy();
		return;
	}

	UTrackEdgeSubsystem* Subsystem =
	GI->GetSubsystem<UTrackEdgeSubsystem>();

	if (!Subsystem)
	{
		OnFail.Broadcast(TEXT("TrackEdge Subsystem not found"));
		SetReadyToDestroy();
		return;
	}

	TMap<FString, FString> Props;
	Props.Add(TEXT("screen"), TEXT("gameplay"));

	Subsystem->TrackEvent(
		EventName,
		Props,
		[WeakThis = TWeakObjectPtr<UTrackEdgeAsyncTrackEvent>(this)]
		(bool bSuccess, const FString& Response)
		{
			if (!WeakThis.IsValid()) return;

			if (bSuccess)
				WeakThis->OnSuccess.Broadcast(Response),
			    WeakThis->SetReadyToDestroy();
			else
				WeakThis->OnFail.Broadcast(Response);
		}
	);
}