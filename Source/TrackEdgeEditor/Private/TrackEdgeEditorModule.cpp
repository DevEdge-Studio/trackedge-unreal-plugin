// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#include "TrackEdgeEditorModule.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IHttpRequest.h"
#include "TrackEdgeSettings.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"



#define LOCTEXT_NAMESPACE "FTrackEdgeEditorModule"

// FORWARD DECLARATION

bool CaptureMap_Editor(
	UWorld* World,
	int32 GridRows,
	int32 GridCols,
	float CameraHeight,
	const FString& SessionFolder,
	ETrackEdgeCaptureMode CaptureMode
);

// STARTUP

void FTrackEdgeEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this,
			&FTrackEdgeEditorModule::RegisterMenus
		)
	);
}

// SHUTDOWN

void FTrackEdgeEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

// REGISTER MENU

void FTrackEdgeEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu =
		UToolMenus::Get()->ExtendMenu(
			"LevelEditor.MainMenu.Window"
		);

	FToolMenuSection& Section =
		Menu->AddSection(
			"TrackEdge",
			LOCTEXT("TrackEdgeSection", "TrackEdge")
		);

	Section.AddMenuEntry(
		"CaptureMap",

		LOCTEXT(
			"CaptureMapLabel",
			"Capture Map"
		),

		LOCTEXT(
			"CaptureMapTooltip",
			"Open TrackEdge Capture Tool"
		),

		FSlateIcon(),

		FUIAction(
			FExecuteAction::CreateRaw(
				this,
				&FTrackEdgeEditorModule::OnCaptureMapClicked
			)
		)
	);
}

// ASYNC START

void FTrackEdgeEditorModule::StartAsyncCapture()
{
	if (!ActiveWorld)
	{
		return;
	}

	CaptureStatusText =
		FText::FromString(TEXT("Capturing..."));

	CurrentCaptureIndex = 0;

	TotalCaptureCount =
		ActiveRows * ActiveCols;

	ActiveWorld->GetTimerManager().SetTimer(
		CaptureTimerHandle,
		FTimerDelegate::CreateRaw(
			this,
			&FTrackEdgeEditorModule::ProcessNextCaptureTile
		),
		0.01f,
		true
	);
}

// PROCESS TILE

void FTrackEdgeEditorModule::ProcessNextCaptureTile()
{
	if (!ActiveWorld)
	{
		FinishCapture();
		return;
	}

	if (CurrentCaptureIndex >= TotalCaptureCount)
	{
		FinishCapture();
		return;
	}

	CurrentCaptureIndex++;

	CaptureProgress =
		(float)CurrentCaptureIndex /
		(float)TotalCaptureCount;

	CaptureStatusText =
		FText::FromString(
			FString::Printf(
				TEXT("Capturing %d / %d"),
				CurrentCaptureIndex,
				TotalCaptureCount
			)
		);
	
	// TEMPORARY SINGLE TILE CAPTURE

	if (CurrentCaptureIndex == 1)
	{
		const bool bCaptureSuccess =
	CaptureMap_Editor(
		ActiveWorld,
		ActiveRows,
		ActiveCols,
		ActiveHeight,
		ActiveSessionFolder,
		CaptureMode
	);

		if (!bCaptureSuccess)
		{
			bIsCapturing = false;

			bCaptureCompleted = false;

			bCaptureFailed = true;

			CaptureProgress = 0.0f;

			CaptureStatusText =
				FText::FromString(TEXT("Capture Failed"));

			if (ActiveWorld)
			{
				ActiveWorld->GetTimerManager().ClearTimer(
					CaptureTimerHandle
				);
			}

			return;
		}
	}

	FSlateApplication::Get().Tick();
	FSlateApplication::Get().PumpMessages();
}

// FINISH

void FTrackEdgeEditorModule::FinishCapture()
{
	if (ActiveWorld)
	{
		ActiveWorld->GetTimerManager().ClearTimer(
			CaptureTimerHandle
		);
	}

	bIsCapturing = false;

	bCaptureCompleted = true;

	CaptureProgress = 1.0f;

	CaptureStatusText =
		FText::FromString(TEXT("Completed"));

	CapturedImagePaths.Empty();

	IFileManager& FileManager =
		IFileManager::Get();

	FileManager.FindFilesRecursive(
		CapturedImagePaths,
		*ActiveSessionFolder,
		TEXT("*.png"),
		true,
		false
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Found %d Captured Images"),
		CapturedImagePaths.Num()
	);

	 CapturedImagePaths.Sort();
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Grid Capture Complete")
	);
}

// CAPTURE WINDOW

void FTrackEdgeEditorModule::OnCaptureMapClicked()
{
	if (!GEditor)
	{
		return;
	}

	bIsCapturing = false;
	bIsUploading = false;
	bCaptureCompleted = false;
	bCaptureFailed = false;
	CurrentCaptureIndex = 0;
	CurrentUploadIndex = 0;
	CaptureProgress = 0.0f;
	UploadProgress = 0.0f;

	CapturedImagePaths.Empty();

	CaptureStatusText =
		FText::FromString(TEXT("Ready"));

	UploadStatusText =
		FText::FromString(TEXT("Ready"));

	UWorld* World =
		GEditor->GetEditorWorldContext().World();

	if (!World)
	{
		return;
	}

	// SELECTED MAP TYPE

	TSharedPtr<FString> SelectedMapType =
		MakeShared<FString>("LEVEL");
	
	TSharedPtr<ETrackEdgeCaptureMode> SelectedCaptureMode =
	MakeShared<ETrackEdgeCaptureMode>(
		ETrackEdgeCaptureMode::Lit
	);

	// WINDOW

	TSharedRef<SWindow> Window =
		SNew(SWindow)

		.Title(FText::FromString("TrackEdge"))

		.ClientSize(FVector2D(520.f, 550.f))

		.SupportsMinimize(false)
		.SupportsMaximize(false);

	// INPUTS

	TSharedPtr<SEditableTextBox> MapNameBox;
	TSharedPtr<SEditableTextBox> DescriptionBox;

	TSharedPtr<SSpinBox<int32>> RowsBox;
	TSharedPtr<SSpinBox<int32>> ColsBox;

	TSharedPtr<SSpinBox<float>> HeightBox;
	
	// WINDOW CONTENT

	Window->SetContent(

		SNew(SBorder)

		.BorderImage(
			FAppStyle::GetBrush("ToolPanel.GroupBorder")
		)

		.Padding(16.f)

		[
			SNew(SVerticalBox)

			// TITLE

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 16)

			[
				SNew(STextBlock)
				.Text(FText::FromString("TrackEdge"))
				.Font(
					FCoreStyle::GetDefaultFontStyle(
						"Bold",
						24
					)
				)
			]

			// MAP NAME

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Map Name"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 12)
			[
				SAssignNew(MapNameBox, SEditableTextBox)
				.Text(FText::FromString("Main menu"))
			]

			// MAP TYPE

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Map Type"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 12)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0, 0, 5, 0)
				[
					SNew(SButton)

					.ButtonColorAndOpacity_Lambda(
						[SelectedMapType]()
						{
							return *SelectedMapType == "LEVEL"
								? FLinearColor(0.0f, 0.35f, 0.2f)
								: FLinearColor(0.15f, 0.15f, 0.15f);
						}
					)

					.Text(FText::FromString("LEVEL"))

					.OnClicked_Lambda(
						[SelectedMapType]()
						{
							*SelectedMapType = "LEVEL";
							return FReply::Handled();
						}
					)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(5, 0, 0, 0)
				[
					SNew(SButton)

					.ButtonColorAndOpacity_Lambda(
						[SelectedMapType]()
						{
							return *SelectedMapType == "UI"
								? FLinearColor(0.0f, 0.35f, 0.2f)
								: FLinearColor(0.15f, 0.15f, 0.15f);
						}
					)

					.Text(FText::FromString("UI"))

					.OnClicked_Lambda(
						[SelectedMapType]()
						{
							*SelectedMapType = "UI";
							return FReply::Handled();
						}
					)
				]
			]

			// DESCRIPTION

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Description"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 12)
			[
				SAssignNew(
					DescriptionBox,
					SEditableTextBox
				)
				.Text(FText::FromString("Optional short note"))
			]

			// ROWS

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Grid Rows"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 12)
			[
				SAssignNew(RowsBox, SSpinBox<int32>)
				.MinValue(1)
				.MaxValue(100)
				.Value(3)
			]

			// COLS

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Grid Columns"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 12)
			[
				SAssignNew(ColsBox, SSpinBox<int32>)
				.MinValue(1)
				.MaxValue(100)
				.Value(3)
			]

			// HEIGHT

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Camera Height"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 20)
			[
				SAssignNew(HeightBox, SSpinBox<float>)
				.MinValue(100.f)
				.MaxValue(100000.f)
				.Value(1000.f)
			]

			// CAPTURE MODE

+ SVerticalBox::Slot()
.AutoHeight()
[
	SNew(STextBlock)
	.Text(FText::FromString("Capture Mode"))
]

+ SVerticalBox::Slot()
.AutoHeight()
.Padding(0, 4, 0, 20)
[
	SNew(SHorizontalBox)

	// LIT

	+ SHorizontalBox::Slot()
	.FillWidth(1.f)
	.Padding(0, 0, 5, 0)

	[
		SNew(SButton)

		.ButtonColorAndOpacity_Lambda(
			[SelectedCaptureMode]()
			{
				return *SelectedCaptureMode ==
					ETrackEdgeCaptureMode::Lit

					? FLinearColor(0.0f, 0.35f, 0.2f)
					: FLinearColor(0.15f, 0.15f, 0.15f);
			}
		)

		.Text(FText::FromString("Lit"))

		.OnClicked_Lambda(
			[SelectedCaptureMode]()
			{
				*SelectedCaptureMode =
					ETrackEdgeCaptureMode::Lit;

				return FReply::Handled();
			}
		)
	]

	// UNLIT

	+ SHorizontalBox::Slot()
	.FillWidth(1.f)
	.Padding(5, 0, 0, 0)

	[
		SNew(SButton)

		.ButtonColorAndOpacity_Lambda(
			[SelectedCaptureMode]()
			{
				return *SelectedCaptureMode ==
					ETrackEdgeCaptureMode::Unlit

					? FLinearColor(0.0f, 0.35f, 0.2f)
					: FLinearColor(0.15f, 0.15f, 0.15f);
			}
		)

		.Text(FText::FromString("Unlit"))

		.OnClicked_Lambda(
			[SelectedCaptureMode]()
			{
				*SelectedCaptureMode =
					ETrackEdgeCaptureMode::Unlit;

				return FReply::Handled();
			}
		)
	]
]
			// PROGRESS TEXT

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 10, 0, 4)
			[
				SAssignNew(ProgressText, STextBlock)
				.Text_Lambda([this]()
				{
					return CaptureStatusText;
				})
			]

			// PROGRESS BAR

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SAssignNew(ProgressBar, SProgressBar)
				.Percent_Lambda([this]()
				{
					return CaptureProgress;
				})
			]

			// BUTTONS

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// CANCEL

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.Text(FText::FromString("Cancel"))
					.OnClicked_Lambda(
						[Window]()
						{
							Window->RequestDestroyWindow();
							return FReply::Handled();
						}
					)
				]
                // OPEN FOLDER BUTTON 
				
				+ SHorizontalBox::Slot()
                .FillWidth(1.f)
                .Padding(6, 0, 6, 0)

               [
	             SAssignNew(OpenFolderButton, SButton)

               	.IsEnabled_Lambda([this]()
	          {
               		return bCaptureCompleted &&
	                       !bIsCapturing &&
	                       !bIsUploading;
	           })

	           .OnClicked_Lambda([this]()
	   {
		       if (!ActiveSessionFolder.IsEmpty())
		   {
		       	const FString AbsolutePath =
	            FPaths::ConvertRelativePathToFull(
		        ActiveSessionFolder
	        );

  FPlatformProcess::ExploreFolder(
	  *AbsolutePath
  );
		   	UE_LOG(
		   LogTemp,
		   Warning,
		   TEXT("Opened Folder: %s"),
		   *AbsolutePath
	   );
		}

		return FReply::Handled();
	})

	[
		SNew(STextBlock)
		.Text(FText::FromString("Open Folder"))
	]
]
	// ADD UPLOAD BUTTON
				
	+ SHorizontalBox::Slot()
    .FillWidth(1.f)
    .Padding(6, 0, 6, 0)

[
	SAssignNew(UploadButton, SButton)

	.IsEnabled_Lambda([this]()
	{
		return bCaptureCompleted &&
			  !bIsCapturing &&
	          !bIsUploading;
	})

	.OnClicked_Lambda([this]()
	{
		UploadMapSet();

		return FReply::Handled();
	})

	[
		SNew(STextBlock)
		.Text(FText::FromString("Upload"))
	]
]
				
				// CREATE MAP

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(6, 0, 0, 0)
				[
					SAssignNew(CreateButton, SButton)

					.IsEnabled_Lambda([this]()
					{
						return !bIsCapturing && !bIsUploading;
					})

					.OnClicked_Lambda(
						[
							this,
							MapNameBox,
							DescriptionBox,
							SelectedMapType,
							SelectedCaptureMode,
							RowsBox,
							ColsBox,
							HeightBox,
							World
						]()
						{
							bIsCapturing = true;
							bCaptureCompleted = false;
							CaptureProgress = 0.0f;

							CaptureStatusText =
								FText::FromString(TEXT("Starting Capture..."));

							const FString MapName =
								MapNameBox->GetText().ToString();

							const FString Description =
								DescriptionBox->GetText().ToString();

							const int32 Rows =
								RowsBox->GetValue();

							const int32 Cols =
								ColsBox->GetValue();

							const float Height =
								HeightBox->GetValue();

							const FString TimeStamp =
								FDateTime::Now().ToString();

							const FString SessionFolder =
								FPaths::ProjectSavedDir()
								/ TEXT("TrackEdgeCaptures")
								/ (MapName + TEXT("_") + TimeStamp);

							IFileManager::Get().MakeDirectory(
								*SessionFolder,
								true
							);

							const FString Json =
								FString::Printf(
									TEXT("{\n")
									TEXT("\"name\":\"%s\",\n")
									TEXT("\"type\":\"%s\",\n")
									TEXT("\"description\":\"%s\",\n")
									TEXT("\"rows\":%d,\n")
									TEXT("\"cols\":%d,\n")
									TEXT("\"cameraHeight\":%.2f\n")
									TEXT("}\n"),

									*MapName,
									**SelectedMapType,
									*Description,
									Rows,
									Cols,
									Height
								);

							FFileHelper::SaveStringToFile(
								Json,
								*(SessionFolder / TEXT("metadata.json"))
							);

							ActiveWorld = World;

							ActiveRows = Rows;
							ActiveCols = Cols;

							ActiveHeight = Height;

							ActiveSessionFolder = SessionFolder;
							
							CurrentMapName = MapName;
                            CurrentDescription = Description;
                            CurrentMapType = *SelectedMapType;
							CaptureMode = *SelectedCaptureMode;
							
							StartAsyncCapture();

							return FReply::Handled();
						}
					)

					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (bIsCapturing)
							{
								return FText::FromString("Capturing...");
							}

							return FText::FromString("Create Map");
						})
					]
				]
			]
		]
	);

	FSlateApplication::Get().AddWindow(Window);
}

// Pop Upload


void FTrackEdgeEditorModule::ShowUploadProgressWindow()
{
	UploadProgress = 0.0f;

	UploadStatusText =
		FText::FromString(TEXT("Preparing Upload..."));

	UploadProgressWindow =
		SNew(SWindow)

		.Title(FText::FromString("Uploading"))
		.ClientSize(FVector2D(320.f, 90.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize);

	UploadProgressWindow->SetContent(

		SNew(SBorder)
		.Padding(12)

		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)

			[
				SAssignNew(
					UploadProgressText,
					STextBlock
				)

				.Text_Lambda([this]()
				{
					return UploadStatusText;
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()

			[
				SAssignNew(
					UploadProgressBar,
					SProgressBar
				)

				.Percent_Lambda([this]()
				{
					return UploadProgress;
				})
			]
		]
	);

	FSlateApplication::Get().AddWindow(
		UploadProgressWindow.ToSharedRef()
	);
}


// UPLOAD MODULE

void FTrackEdgeEditorModule::UploadMapSet()
{
	
	bIsUploading = true;
	ShowUploadProgressWindow();
	
	const UTrackEdgeSettings* Settings =
		GetDefault<UTrackEdgeSettings>();

	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("TrackEdge Settings Missing"));
		return;
	}

	FString Url =
		Settings->BaseUrl +
		TEXT("/api/v1/maps/projects/") +
		Settings->ProjectId +
		TEXT("/map-sets");

	TSharedPtr<FJsonObject> JsonObject =
		MakeShared<FJsonObject>();

	JsonObject->SetStringField(
	TEXT("name"),
	CurrentMapName
);

	JsonObject->SetStringField(
		TEXT("slug"),
		CurrentMapName.Replace(TEXT(" "), TEXT("-")).ToLower()
	);

	JsonObject->SetStringField(
		TEXT("type"),
		CurrentMapType
	);

	JsonObject->SetStringField(
		TEXT("description"),
		CurrentDescription
	);

	JsonObject->SetNumberField(
		TEXT("gridRows"),
		ActiveRows
	);

	JsonObject->SetNumberField(
		TEXT("gridCols"),
		ActiveCols
	);

	JsonObject->SetNumberField(
		TEXT("originX"),
		0
	);

	JsonObject->SetNumberField(
	    TEXT("normalizedWidth"),
	    ActiveCols * CaptureTileWidth
    );

	JsonObject->SetNumberField(
		TEXT("normalizedHeight"),
		ActiveRows * CaptureTileHeight
	);
	
	JsonObject->SetNumberField(
		TEXT("originY"),
		0
	);

	FString OutputString;

	TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&OutputString);

	FJsonSerializer::Serialize(
		JsonObject.ToSharedRef(),
		Writer
	);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);

	Request->SetVerb(TEXT("POST"));

	Request->SetHeader(
		TEXT("Content-Type"),
		TEXT("application/json")
	);

	Request->SetHeader(
		TEXT("x-api-key"),
		Settings->ApiKey
	);

	Request->SetContentAsString(OutputString);

	Request->OnProcessRequestComplete().BindRaw(
		this,
		&FTrackEdgeEditorModule::OnCreateMapSetResponse
	);

	Request->ProcessRequest();

	UE_LOG(LogTemp, Warning, TEXT("Creating Map Set..."));
	
}

    //MAP UPLOADS

void FTrackEdgeEditorModule::RequestUploadUrl(
const FString& FilePath
)
{
	const UTrackEdgeSettings* Settings =
		GetDefault<UTrackEdgeSettings>();

	if (!Settings)
	{
		return;
	}

	FString Url =
		Settings->BaseUrl +
		TEXT("/api/v1/uploads/presigned-url");

	TSharedPtr<FJsonObject> JsonObject =
		MakeShared<FJsonObject>();

	FString FileName =
		FPaths::GetCleanFilename(FilePath);

	int64 FileSize =
		IFileManager::Get().FileSize(*FilePath);

	JsonObject->SetStringField(
		TEXT("projectId"),
		Settings->ProjectId
	);

	JsonObject->SetStringField(
		TEXT("fileName"),
		FileName
	);

	JsonObject->SetStringField(
		TEXT("contentType"),
		TEXT("image/png")
	);

	JsonObject->SetNumberField(
		TEXT("sizeBytes"),
		FileSize
	);

	JsonObject->SetStringField(
		TEXT("folder"),
		TEXT("maps")
	);

	FString JsonString;

	TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&JsonString);

	FJsonSerializer::Serialize(
		JsonObject.ToSharedRef(),
		Writer
	);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);

	Request->SetVerb(TEXT("POST"));

	Request->SetHeader(
		TEXT("Content-Type"),
		TEXT("application/json")
	);

	Request->SetHeader(
		TEXT("x-api-key"),
		Settings->ApiKey
	);

	Request->SetContentAsString(JsonString);

	Request->OnProcessRequestComplete().BindRaw(
		this,
		&FTrackEdgeEditorModule::OnUploadUrlResponse
	);

	Request->ProcessRequest();

	UE_LOG(LogTemp, Warning, TEXT("Requesting Upload URL..."));
}

void FTrackEdgeEditorModule::OnUploadUrlResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful
)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		bIsUploading = false;

		UploadStatusText =
			FText::FromString(TEXT("Upload Failed"));

		UE_LOG(LogTemp, Error, TEXT("Upload URL Request Failed"));

		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(
				"Failed to request upload URL.\nPlease try again."
			)
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Upload URL Response: %s"),
		*Response->GetContentAsString()
	);

	TSharedPtr<FJsonObject> JsonObject;

	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(
			Response->GetContentAsString()
		);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed To Parse Upload URL Response"));
		return;
	}

	TSharedPtr<FJsonObject> DataObject =
		JsonObject->GetObjectField(TEXT("data"));

	FString PresignedUrl =
		DataObject->GetStringField(TEXT("presignedUrl"));

	FString UploadId =
		DataObject->GetStringField(TEXT("uploadId"));

	if (!CapturedImagePaths.IsValidIndex(CurrentUploadIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Upload Index"));
		return;
	}

	UploadImageToS3(
		CapturedImagePaths[CurrentUploadIndex],
		PresignedUrl,
		UploadId
	);
}

   void FTrackEdgeEditorModule::UploadImageToS3(
	const FString& FilePath,
	const FString& PresignedUrl,
	const FString& UploadId
)
{
	TArray<uint8> FileData;

	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed To Read PNG File"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(PresignedUrl);

	Request->SetVerb(TEXT("PUT"));

	Request->SetHeader(
		TEXT("Content-Type"),
		TEXT("image/png")
	);

	Request->SetContent(FileData);

	Request->OnProcessRequestComplete().BindLambda(
		[this, UploadId, FilePath]
		(
			FHttpRequestPtr Req,
			FHttpResponsePtr Res,
			bool bSuccess
		)
		{
			OnS3UploadComplete(
				Req,
				Res,
				bSuccess,
				UploadId,
				FilePath
			);
		}
	);

	Request->ProcessRequest();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Uploading PNG To S3...")
	);
}

// S3 COMPLETE CALLBACK
void FTrackEdgeEditorModule::OnS3UploadComplete(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful,
	FString UploadId,
	FString FilePath
)
{
	if (!bWasSuccessful)
	{
		int32& RetryCount =
			UploadRetryCounts.FindOrAdd(FilePath);

		RetryCount++;

		UE_LOG(
			LogTemp,
			Error,
			TEXT("S3 Upload Failed (%d/4): %s"),
			RetryCount,
			*FilePath
		);

		if (RetryCount < 4)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Retrying Upload In 2 Seconds...")
			);

			if (ActiveWorld)
			{
				ActiveWorld->GetTimerManager().SetTimer(
					UploadRetryTimerHandle,

					FTimerDelegate::CreateLambda(
						[this, FilePath]()
						{
							RequestUploadUrl(FilePath);
						}
					),

					2.0f,
					false
				);
			}

			return;
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("Upload Permanently Failed After 4 Attempts")
		);

		CurrentUploadIndex++;

		if (CapturedImagePaths.IsValidIndex(CurrentUploadIndex))
		{
			RequestUploadUrl(
				CapturedImagePaths[CurrentUploadIndex]
			);
		}

		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("S3 Upload Success!"));
	
	UploadRetryCounts.Remove(FilePath);

	CreateMapEntry(
		UploadId,
		FilePath
	);
}

// CREAT MAP ENTRY

void FTrackEdgeEditorModule::CreateMapEntry(
	const FString& UploadId,
	const FString& FilePath
)
{
	const UTrackEdgeSettings* Settings =
		GetDefault<UTrackEdgeSettings>();

	if (!Settings)
	{
		return;
	}

	FString Url =
		Settings->BaseUrl +
		TEXT("/api/v1/maps/projects/") +
		Settings->ProjectId +
		TEXT("/map-sets/") +
		CurrentMapSetId +
		TEXT("/maps");

	TSharedPtr<FJsonObject> JsonObject =
		MakeShared<FJsonObject>();

	const FString FileName =
		FPaths::GetBaseFilename(FilePath);
	
     // Row & Col
     
	FString CleanName =
	FPaths::GetBaseFilename(FilePath);

	FString Left;
	FString Right;

	CleanName.Split(TEXT("_"), nullptr, &Right);

	FString RowString;
	FString ColString;

	Right.Split(TEXT("_"), &RowString, &ColString);

	const int32 Row =
		FCString::Atoi(*RowString);

	const int32 Col =
		FCString::Atoi(*ColString);
	
	// End Row & Col

	JsonObject->SetStringField(
		TEXT("name"),
		FileName
	);

	JsonObject->SetStringField(
		TEXT("mediaType"),
		TEXT("IMAGE")
	);

	JsonObject->SetStringField(
		TEXT("uploadId"),
		UploadId
	);

	const int32 UploadRow = Col;
	const int32 UploadCol = Row;

	JsonObject->SetNumberField(
		TEXT("rowIndex"),
		UploadRow
	);

	JsonObject->SetNumberField(
		TEXT("colIndex"),
		UploadCol
	);
	
	UE_LOG(
	LogTemp,
	Warning,
	TEXT("Tile %s -> Original(%d,%d) Upload(%d,%d)"),
	*FileName,
	Row,
	Col,
	UploadRow,
	UploadCol
);

	JsonObject->SetNumberField(
		TEXT("sortOrder"),
		CurrentUploadIndex
	);

	JsonObject->SetNumberField(
		TEXT("width"),
		CaptureTileWidth
	);

	JsonObject->SetNumberField(
		TEXT("height"),
		CaptureTileHeight
	);

	JsonObject->SetNumberField(TEXT("boundsX"), 0);
	JsonObject->SetNumberField(TEXT("boundsY"), 0);
	JsonObject->SetNumberField(TEXT("boundsWidth"), 0);
	JsonObject->SetNumberField(TEXT("boundsHeight"), 0);

	FString JsonString;

	TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&JsonString);

	FJsonSerializer::Serialize(
		JsonObject.ToSharedRef(),
		Writer
	);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);

	Request->SetVerb(TEXT("POST"));

	Request->SetHeader(
		TEXT("Content-Type"),
		TEXT("application/json")
	);

	Request->SetHeader(
		TEXT("x-api-key"),
		Settings->ApiKey
	);

	Request->SetContentAsString(JsonString);

	Request->OnProcessRequestComplete().BindRaw(
		this,
		&FTrackEdgeEditorModule::OnCreateMapEntryResponse
	);

	Request->ProcessRequest();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Creating Map Entry...")
	);
}

// MAP ENTRY RESPONSE

void FTrackEdgeEditorModule::OnCreateMapEntryResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful
)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Map Entry Creation Failed"));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;

	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(
			Response->GetContentAsString()
		);

	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		TSharedPtr<FJsonObject> DataObject =
			JsonObject->GetObjectField(TEXT("data"));

		CurrentCreatedMapId =
			DataObject->GetStringField(TEXT("id"));

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Created Map Id = %s"),
			*CurrentCreatedMapId
		);
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Map Entry Created: %s"),
		*Response->GetContentAsString()
	);

	CurrentUploadIndex++;
	
	UploadProgress =
	(float)CurrentUploadIndex /
	(float)CapturedImagePaths.Num();

	UploadStatusText =
		FText::FromString(
			FString::Printf(
				TEXT("Uploading %d / %d"),
				CurrentUploadIndex,
				CapturedImagePaths.Num()
			)
		);

	FSlateApplication::Get().Tick();
	FSlateApplication::Get().PumpMessages();

	if (CapturedImagePaths.IsValidIndex(CurrentUploadIndex))
	{
		RequestUploadUrl(
			CapturedImagePaths[CurrentUploadIndex]
		);
	}
	else
	{
		SaveLevelMapping();
		
		bIsUploading = false;

		UploadProgress = 1.0f;

		UploadStatusText =
			FText::FromString(TEXT("Upload Complete"));

		if (UploadProgressWindow.IsValid())
		{
			FTimerHandle TempHandle;

			ActiveWorld->GetTimerManager().SetTimer(
				TempHandle,

				FTimerDelegate::CreateLambda(
					[this]()
					{
						if (UploadProgressWindow.IsValid())
						{
							UploadProgressWindow->RequestDestroyWindow();
						}
					}
					
				),

				2.0f,
				false
			);
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("ALL MAPS UPLOADED!")
		);
	}
}

// UPLOAD RESPONSE CALLBACK

void FTrackEdgeEditorModule::OnCreateMapSetResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful
)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		bIsUploading = false;

		UploadProgress = 0.0f;

		UploadStatusText =
			FText::FromString(TEXT("Upload Failed"));

		UE_LOG(LogTemp, Error, TEXT("Map Set Request Failed"));

		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(
				"Failed to create Map Set.\nPlease try again."
			)
		);

		return;
	}

	TSharedPtr<FJsonObject> JsonObject;

	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(
			Response->GetContentAsString()
		);
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Response: %s"),
		*Response->GetContentAsString()
	);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse server response."));
		return;
	}

	// Backend returned success = false
	bool bSuccess = JsonObject->GetBoolField(TEXT("success"));

	if (!bSuccess)
	{
		FString Message =
			JsonObject->GetStringField(TEXT("message"));

		bIsUploading = false;
		UploadProgress = 0.f;
		UploadStatusText =
			FText::FromString(TEXT("Upload Failed"));

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("TrackEdge: %s"),
			*Message
		);

		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(
				FString::Printf(
					TEXT("%s\n\nUpgrade to TrackEdge Pro to upload more maps."),
					*Message
				)
			)
		);

		return;
	}

	// Success path
	TSharedPtr<FJsonObject> DataObject =
		JsonObject->GetObjectField(TEXT("data"));

	CurrentMapSetId =
		DataObject->GetStringField(TEXT("id"));

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("MapSetId: %s"),
		*CurrentMapSetId
	);

	CurrentUploadIndex = 0;

	if (CapturedImagePaths.Num() > 0)
	{
		RequestUploadUrl(
			CapturedImagePaths[0]
		);
	}
	
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(
	FTrackEdgeEditorModule,
	TrackEdgeEditor
)

void FTrackEdgeEditorModule::SaveLevelMapping()
{
	UTrackEdgeSettings* Settings =
		GetMutableDefault<UTrackEdgeSettings>();

	if (!Settings)
	{
		return;
	}

	FTrackEdgeLevelMapping Mapping;

	Mapping.LevelName =
		ActiveWorld->GetMapName();

	Mapping.MapSetId =
		CurrentMapSetId;

	Mapping.MapId =
		CurrentCreatedMapId;

	Settings->LevelMappings.Add(Mapping);

	Settings->SaveConfig();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Level Mapping Saved")
	);
}