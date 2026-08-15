// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TrackEdgeAsyncStartReplay.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTrackEdgeReplayResult,
	const FString&,
	Response
);

UCLASS()
class TRACKEDGE_API UTrackEdgeAsyncStartReplay
	: public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FTrackEdgeReplayResult OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FTrackEdgeReplayResult OnFail;

	UFUNCTION(
		BlueprintCallable,
		meta=(BlueprintInternalUseOnly="true",
		WorldContext="WorldContextObject"),
		Category="TrackEdge|Replay"
	)
	static UTrackEdgeAsyncStartReplay* StartReplaySession(
		UObject* WorldContextObject
	);

	virtual void Activate() override;

private:

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject = nullptr;
};