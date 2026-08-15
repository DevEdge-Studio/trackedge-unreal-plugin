// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"

class SButton;
class SProgressBar;
class STextBlock;

enum class ETrackEdgeCaptureMode : uint8
{
	Lit,
	Unlit
};

class FTrackEdgeEditorModule : public IModuleInterface
{
public:

    // UPLOAD PROGRESS BAR
    
	bool bIsUploading = false;
    float UploadProgress = 0.0f;
    FText UploadStatusText;
    TSharedPtr<SWindow> UploadProgressWindow;
    TSharedPtr<SProgressBar> UploadProgressBar;
    TSharedPtr<STextBlock> UploadProgressText;
	
	// FOLDER LOCATING & UPLOAD BUTTON

	TSharedPtr<class SButton> OpenFolderButton;
	TSharedPtr<class SButton> UploadButton;

	bool bCaptureCompleted = false;
	bool bCaptureFailed = false;

	// CAPTURE STATE

	FTimerHandle CaptureTimerHandle;
	FTimerHandle UploadRetryTimerHandle;

	int32 CurrentCaptureIndex = 0;
	int32 TotalCaptureCount = 0;

	TArray<FVector> CaptureLocations;

	FString ActiveSessionFolder;
	TArray<FString> CapturedImagePaths;
	
	TMap<FString, int32> UploadRetryCounts;

	int32 CurrentUploadIndex = 0;
	
	UWorld* ActiveWorld = nullptr;

	int32 ActiveRows = 0;
	int32 ActiveCols = 0;

	float ActiveHeight = 1000.f;
	
	ETrackEdgeCaptureMode CaptureMode =
	ETrackEdgeCaptureMode::Lit;
	
	// UI STATE

	float CaptureProgress = 0.0f;
	FText CaptureStatusText;
	bool bIsCapturing = false;
	bool bCancelCapture = false;
	
	// UI REFERENCES

	TSharedPtr<SButton> CreateButton;
	TSharedPtr<SButton> CancelButton;
	TSharedPtr<SProgressBar> ProgressBar;
	TSharedPtr<STextBlock> ProgressText;


	// ASYNC FUNCTIONS

	void StartAsyncCapture();
	void ShowUploadProgressWindow();
	void ProcessNextCaptureTile();
	
	void CaptureSingleTile(
	int32 Row,
	int32 Col
);

	void FinishCapture();

	// MODULE
	
	int32 CurrentRow = 0;
	int32 CurrentCol = 0;
	
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	// UPLOADING

	void UploadMapSet();

	void OnCreateMapSetResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful
	);

	void RequestUploadUrl(const FString& FilePath);

	void OnUploadUrlResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful
	);

	void UploadImageToS3(
		const FString& FilePath,
		const FString& PresignedUrl,
		const FString& UploadId
	);

	void OnS3UploadComplete(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FString UploadId,
		FString FilePath
	);

	void CreateMapEntry(
	const FString& UploadId,
	const FString& FilePath
     );
	
	void OnCreateMapEntryResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful
	);
	
	// CURRENT MAP DATA

	FString CurrentMapSetId;
	FString CurrentMapName;
	FString CurrentDescription;
	FString CurrentMapType;
	FString CurrentMapSet;
	FString CurrentCreatedMapId;
	
	int32 CaptureTileWidth = 1024;
	int32 CaptureTileHeight = 1024;

private:

	void RegisterMenus();
	void SaveLevelMapping();
	void OnCaptureMapClicked();
};