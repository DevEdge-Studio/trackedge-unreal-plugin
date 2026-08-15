// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "TrackEdgeReplayTypes.generated.h"

UENUM(BlueprintType)
enum class ETrackEdgeReplayEventType : uint8
{
	PLAYER_START      UMETA(DisplayName="Player Start"),
	SESSION_REPLAY    UMETA(DisplayName="Session Replay"),
	PLAYER_DEATH      UMETA(DisplayName="Player Death"),
	PLAYER_LEAVE      UMETA(DisplayName="Player Leave"),
	CUSTOM_EVENT      UMETA(DisplayName="Custom Event")
};

USTRUCT(BlueprintType)
struct FTrackEdgeReplayPoint
{
	GENERATED_BODY()

	UPROPERTY()
	FString Time;

	UPROPERTY()
	float X = 0.f;

	UPROPERTY()
	float Y = 0.f;

	UPROPERTY()
	float Z = 0.f;

	UPROPERTY()
	float Yaw = 0.f;

	UPROPERTY()
	float Pitch = 0.f;
	
	UPROPERTY()
	float NormalizedX = 0.f;

	UPROPERTY()
	float NormalizedY = 0.f;
	
	UPROPERTY()
    float NormalizedZ = 0.f;
	
	float Cpu = 0.f;
	float Gpu = 0.f;
	float Ram = 0.f;
	float Vram = 0.f;

	int32 FPS = 0;
	int32 FPSDrop = 0;
};