// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TrackEdgeLevelMapping.h"

#include "TrackEdgeSettings.generated.h"

UCLASS(Config=Game, DefaultConfig)
class TRACKEDGE_API UTrackEdgeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	virtual FName GetCategoryName() const override
	{
		return TEXT("Plugins");
	}

	virtual FName GetSectionName() const override
	{
		return TEXT("TrackEdge");
	}

public:

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="TrackEdge")
	FString ProjectId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="TrackEdge")
	FString ApiKey;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="TrackEdge")
	FString BaseUrl = "https://trackedge.dev";
	
	UPROPERTY(
		Config,
		EditAnywhere,
		Category="Level Mappings"
	)
	TArray<FTrackEdgeLevelMapping> LevelMappings;
};