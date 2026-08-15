// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TrackEdgeAsyncEndReplay.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTrackEdgeReplayEndResult,
	const FString&,
	Response
);

UCLASS()
class TRACKEDGE_API UTrackEdgeAsyncEndReplay
	: public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FTrackEdgeReplayEndResult OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FTrackEdgeReplayEndResult OnFail;

	UFUNCTION(
		BlueprintCallable,
		meta=(BlueprintInternalUseOnly="true",
		WorldContext="WorldContextObject"),
		Category="TrackEdge|Replay"
	)
	static UTrackEdgeAsyncEndReplay* EndReplaySession(
		UObject* WorldContextObject
	);

	virtual void Activate() override;

private:

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject = nullptr;
};