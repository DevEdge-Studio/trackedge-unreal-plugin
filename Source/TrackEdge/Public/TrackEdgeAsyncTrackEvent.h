// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TrackEdgeAsyncTrackEvent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTrackEdgeResponse, const FString&, Response);

UCLASS()
class TRACKEDGE_API UTrackEdgeAsyncTrackEvent : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FTrackEdgeResponse OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FTrackEdgeResponse OnFail;

	UFUNCTION(
	BlueprintCallable,Category="TrackEdge",meta=(BlueprintInternalUseOnly="true",WorldContext="WorldContextObject"))
	static UTrackEdgeAsyncTrackEvent* TrackEventAsync(
		UObject* WorldContextObject,
		const FString& EventName
	);

	virtual void Activate() override;

private:

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject = nullptr;
	FString EventName;
};