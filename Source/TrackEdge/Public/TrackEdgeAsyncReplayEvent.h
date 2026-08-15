// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TrackEdgeReplayTypes.h"
#include "TrackEdgeAsyncReplayEvent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTrackEdgeReplayEventFinished);

UCLASS()
class TRACKEDGE_API UTrackEdgeAsyncReplayEvent : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FTrackEdgeReplayEventFinished OnCompleted;

	UFUNCTION(
	BlueprintCallable,
	Category="TrackEdge|Replay",
	meta=(
		BlueprintInternalUseOnly="true",
		WorldContext="WorldContextObject",
		DisplayName="Capture Replay Event"
	)
    )
	static UTrackEdgeAsyncReplayEvent* SetReplayEventType(
		UObject* WorldContextObject,
		ETrackEdgeReplayEventType EventType
	);

	virtual void Activate() override;

private:

	UObject* WorldContextObject;

	ETrackEdgeReplayEventType ReplayEventType;
};