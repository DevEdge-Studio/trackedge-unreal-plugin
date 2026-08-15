// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "TrackEdgeLevelMapping.generated.h"

USTRUCT(BlueprintType)
struct FTrackEdgeLevelMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="TrackEdge")
	FString LevelName;

	UPROPERTY(EditAnywhere, Config, Category="TrackEdge")
	FString MapSetId;

	UPROPERTY(EditAnywhere, Config, Category="TrackEdge")
	FString MapId;

	UPROPERTY(EditAnywhere, Config, Category="Replay")
	float ReplaySampleInterval = 0.2f;

	UPROPERTY(EditAnywhere, Config, Category="Replay")
	int32 ReplayChunkSize = 25;

	UPROPERTY(EditAnywhere,Config,Category="Replay",meta=(DisplayName="Automatic Replay"))
    bool bEnableReplay = true;
};