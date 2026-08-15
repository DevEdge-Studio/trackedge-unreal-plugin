// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TrackEdgeReplayTypes.h"
#include "TimerManager.h"
#include "TrackEdgeLevelMapping.h"
#include "TrackEdgeSubsystem.generated.h"

UCLASS()
class TRACKEDGE_API UTrackEdgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
    
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    
	void TrackEvent(
		const FString& EventName,
		const TMap<FString, FString>& Properties,
		TFunction<void(bool, const FString&)> Callback
	);
    
	void StartReplaySession(
	bool bIgnoreLevelSetting = false,
	TFunction<void(bool,const FString&)> Callback = nullptr
);
    
	void EndReplaySession(
		bool bStartNew = false,
		TFunction<void(bool,const FString&)> Callback = nullptr
	);
	
	void SetReplayEventType(ETrackEdgeReplayEventType NewType);
	
	void RecordReplayPoint();
	void CacheLevelBounds();
	virtual void Deinitialize() override;
	
private:
    
	void LoadOrCreatePlayerId();
	void GenerateFingerprint();
	void InitSession();
	void IdentifyPlayer();
	void StartReplaySession_Internal(
	TFunction<void(bool,const FString&)> Callback = nullptr);
	void UploadReplayChunk();
	void EnsureReplayTracked();
	
	void OnPreLoadMap(const FString& MapName);
	void OnPostLoadMap(UWorld* LoadedWorld);
	
	struct FTrackEdgeRetryState
	{
		FTimerHandle Timer;

		int32 RetryCount = 0;
	};
	
	bool RetryRequest(
	FTrackEdgeRetryState& Retry,
	TFunction<void()> RetryFunction,
	float Delay = 2.f
);
	
	void ResetRetry(FTrackEdgeRetryState& Retry);
	
private:
	
	FString PlayerId;
	FString Fingerprint;
	FString Signature;
	
	// Cached Map Bounds
    
	float MapMinX = 0.f;
	float MapMinY = 0.f;
	float MapMinZ = 0.f;
    
	float MapMaxX = 0.f;
	float MapMaxY = 0.f;
	float MapMaxZ = 0.f;
	
	// Replay State
    
	ETrackEdgeReplayEventType CurrentReplayEventType =
	ETrackEdgeReplayEventType::PLAYER_START;
	
	ETrackEdgeReplayEventType RecordingChunkType =
	ETrackEdgeReplayEventType::PLAYER_START;
	
	FString ReplaySessionId;
	bool bReplayRunning = false;
	bool bReplayUploadInProgress = false;
	int32 ReplayChunkIndex = 1;
	TArray<FTrackEdgeReplayPoint> ReplayBuffer;
	FTimerHandle ReplayTimerHandle;
	const FTrackEdgeLevelMapping* GetCurrentLevelMapping() const;
	
	bool bIsInitialized = false;
	bool bIsIdentified = false; 
	bool bHasTrackedEvent = false;
	
	bool IsReplayEnabledForCurrentLevel() const;
	bool bSubsystemShuttingDown = false;
	
	FTrackEdgeRetryState InitRetry;
	FTrackEdgeRetryState IdentifyRetry;
	FTrackEdgeRetryState TrackRetry;
	FTrackEdgeRetryState ReplayStartRetry;
	FTrackEdgeRetryState ReplayChunkRetry;
	FTrackEdgeRetryState ReplayEndRetry;
	FTrackEdgeRetryState ReplayTrackRetry;
	
	static constexpr int32 MaxRetries = 3;
	
	FString PendingTrackEventName;
	TMap<FString, FString> PendingTrackProperties;
	TFunction<void(bool,const FString&)> PendingTrackCallback;
	
};