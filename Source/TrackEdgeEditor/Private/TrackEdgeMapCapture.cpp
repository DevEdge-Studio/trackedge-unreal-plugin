// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Engine/LevelBounds.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "TrackEdgeEditorModule.h"

bool CaptureMap_Editor(
	UWorld* World,
	int32 GridRows,
	int32 GridCols,
	float CameraHeight,
	const FString& SessionFolder,
	ETrackEdgeCaptureMode CaptureMode
)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("World is null"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("Starting Grid Capture"));
	
	// FIND LEVEL BOUNDS

	ALevelBounds* LevelBoundsActor = nullptr;

	for (TActorIterator<ALevelBounds> It(World); It; ++It)
	{
		LevelBoundsActor = *It;
		break;
	}

	if (!LevelBoundsActor)
	{
		UE_LOG(LogTemp, Error, TEXT("No LevelBounds actor found!"));

		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(
				"No LevelBounds actor found!\n\nPlease add one from Place Actors -> Level Bounds."
			)
		);

		return false;
	}
	
	// GET LEVEL SIZE

	FBox Bounds =
		LevelBoundsActor->GetComponentsBoundingBox(true);

	FVector BoundsMin = Bounds.Min;
	FVector BoundsMax = Bounds.Max;

	float MapWidth =
		BoundsMax.X - BoundsMin.X;

	float MapHeight =
		BoundsMax.Y - BoundsMin.Y;

	UE_LOG(LogTemp, Warning, TEXT("Map Width: %f"), MapWidth);
	UE_LOG(LogTemp, Warning, TEXT("Map Height: %f"), MapHeight);
	
	// TILE SIZE

	float TileWidth =
		MapWidth / GridCols;

	float TileHeight =
		MapHeight / GridRows;
	
	// LOOP GRID
     
	int32 TotalTiles =
	GridRows * GridCols;

	int32 CurrentTile = 0;
	
	IConsoleVariable* LumenGI =
	IConsoleManager::Get().FindConsoleVariable(
		TEXT("r.Lumen.GlobalIllumination"));
	
	
	for (int32 Row = 0; Row < GridRows; Row++)
	{
		for (int32 Col = 0; Col < GridCols; Col++)
			
		{
			CurrentTile++;

			float Progress =
				(float)CurrentTile /
				(float)TotalTiles;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Capture Progress: %d / %d"),
				CurrentTile,
				TotalTiles
			);
			
			FSlateApplication::Get().Tick();
			FSlateApplication::Get().PumpMessages();
		
			float X =
	        BoundsMin.X +
	        (TileWidth * 0.5f) +
	        (Col * TileWidth);

			float Y =
				BoundsMin.Y +
				(TileHeight * 0.5f) +
				(Row * TileHeight);

			FVector CaptureLocation(
				X,
				Y,
				CameraHeight
			);

			FRotator CaptureRotation(
				-90.f,
				0.f,
				0.f
			);
			
			// SPAWN CAPTURE

			ASceneCapture2D* CaptureActor =
				World->SpawnActor<ASceneCapture2D>(
					CaptureLocation,
					CaptureRotation
				);

			if (!CaptureActor)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to spawn capture actor"));
				continue;
			}

			USceneCaptureComponent2D* CaptureComp =
				CaptureActor->GetCaptureComponent2D();
			
			CaptureComp->bCaptureEveryFrame = false;
			CaptureComp->bCaptureOnMovement = false;
			
			// CAMERA SETTINGS

			CaptureComp->ProjectionType =
				ECameraProjectionMode::Orthographic;

			CaptureComp->OrthoWidth =
				TileWidth;
			

			if (CaptureMode ==
				ETrackEdgeCaptureMode::Lit)
			{
				CaptureComp->ProjectionType =
				ECameraProjectionMode::Orthographic;
				CaptureComp->OrthoWidth = TileWidth;

				CaptureComp->CaptureSource =
				ESceneCaptureSource::SCS_FinalColorLDR;

				CaptureComp->bCaptureEveryFrame = false;
				CaptureComp->bCaptureOnMovement = false;
				CaptureComp->PostProcessBlendWeight = 1.0f;
				CaptureComp->PostProcessSettings.bOverride_VignetteIntensity = true;
				CaptureComp->PostProcessSettings.VignetteIntensity = 0.0f;
				CaptureComp->PostProcessSettings.bOverride_SceneFringeIntensity = true;
				CaptureComp->PostProcessSettings.SceneFringeIntensity = 0.0f;
				CaptureComp->PostProcessSettings.bOverride_BloomIntensity = true;
				CaptureComp->PostProcessSettings.BloomIntensity = 0.0f;
				CaptureComp->ShowFlags.SetVignette(false);
				CaptureComp->ShowFlags.SetLensFlares(false);
				CaptureComp->ShowFlags.SetHMDDistortion(false);
			}
			else
			{
				CaptureComp->ProjectionType =
					ECameraProjectionMode::Orthographic;

				CaptureComp->OrthoWidth = TileWidth;

				CaptureComp->CaptureSource =
				ESceneCaptureSource::SCS_FinalColorLDR;
            
            CaptureComp->ShowFlags.SetLighting(true);
            CaptureComp->ShowFlags.SetBloom(false);
            CaptureComp->ShowFlags.SetEyeAdaptation(false);
            CaptureComp->ShowFlags.SetLumenGlobalIllumination(false);
            CaptureComp->ShowFlags.SetLumenReflections(false);
            CaptureComp->ShowFlags.SetAtmosphere(true);
            CaptureComp->ShowFlags.SetVolumetricFog(false);
			CaptureComp->ShowFlags.SetSkyLighting(true);
			CaptureComp->ShowFlags.SetFog(false);	
            CaptureComp->ShowFlags.SetPostProcessing(true);
            CaptureComp->ShowFlags.SetTonemapper(true);
            CaptureComp->PostProcessBlendWeight = 1.0f;
            CaptureComp->PostProcessSettings.bOverride_VignetteIntensity = true;
            CaptureComp->PostProcessSettings.VignetteIntensity = 0.0f;
            CaptureComp->PostProcessSettings.bOverride_BloomIntensity = true;
            CaptureComp->PostProcessSettings.BloomIntensity = 0.0f;
            CaptureComp->PostProcessSettings.bOverride_AutoExposureMethod = true;
				
            CaptureComp->PostProcessSettings.AutoExposureMethod =
            EAutoExposureMethod::AEM_Manual;
				
			CaptureComp->ShowFlags.SetDynamicShadows(false);
			CaptureComp->ShowFlags.SetLighting(false);
            CaptureComp->PostProcessSettings.bOverride_AutoExposureBias = true;
            CaptureComp->PostProcessSettings.AutoExposureBias = 0.0f;
            CaptureComp->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
            CaptureComp->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
            CaptureComp->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
            CaptureComp->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
			}
			
			// EXPOSURE
			
			if (CaptureMode ==
			ETrackEdgeCaptureMode::Lit)
			{CaptureComp->PostProcessSettings
			.bOverride_AutoExposureMethod = true;

				CaptureComp->PostProcessSettings
				.AutoExposureMethod =
				EAutoExposureMethod::AEM_Manual;

				CaptureComp->PostProcessSettings
				.bOverride_AutoExposureBias = true;

				CaptureComp->PostProcessSettings
				.AutoExposureBias = 1.0f;
			}
			else
			{
				CaptureComp->PostProcessSettings
			.bOverride_AutoExposureMethod = true;

				CaptureComp->PostProcessSettings
				.AutoExposureMethod =
				EAutoExposureMethod::AEM_Manual;

				CaptureComp->PostProcessSettings
				.bOverride_AutoExposureBias = true;

				CaptureComp->PostProcessSettings
				.AutoExposureBias = 1.0f;
				
				CaptureComp->PostProcessSettings
				.bOverride_AutoExposureMinBrightness = true;
				
				CaptureComp->PostProcessSettings
				.AutoExposureMinBrightness = 1.0f;
				
				CaptureComp->PostProcessSettings
				.bOverride_AutoExposureMaxBrightness = true;
				
				CaptureComp->PostProcessSettings
				.AutoExposureMaxBrightness = 1.0f;
			}
			
			// RENDER TARGET

			UTextureRenderTarget2D* RenderTarget =
				NewObject<UTextureRenderTarget2D>();
			
			if (CaptureMode ==
			ETrackEdgeCaptureMode::Lit)
			{
				RenderTarget->InitAutoFormat(
				1024,
				1024
			);
				
				RenderTarget->UpdateResourceImmediate(true);
				CaptureComp->TextureTarget =
					RenderTarget;
				
			}
				else
				{
					RenderTarget->RenderTargetFormat =
                                RTF_RGBA8;
                    
                    			RenderTarget->InitCustomFormat(
                    				1024,
                    				1024,
                    				PF_B8G8R8A8,
                    				false
                    			);
                    
                    			RenderTarget->TargetGamma = 2.2f;
					
					RenderTarget->UpdateResourceImmediate(true);

					CaptureComp->TextureTarget =
						RenderTarget;
				}
			
			// CAPTURE
			
			if (CaptureMode ==
			ETrackEdgeCaptureMode::Lit)
			{
				CaptureComp->CaptureScene();
				FlushRenderingCommands();
			}
			else
			{
				IConsoleManager::Get().FindConsoleVariable(
			TEXT("r.EyeAdaptationQuality"))->Set(0);
			
				CaptureComp->CaptureScene();
				FlushRenderingCommands();
			}
			
			// READ PIXELS

			FTextureRenderTargetResource* RTResource =
				RenderTarget->GameThread_GetRenderTargetResource();

			TArray<FColor> PixelData;

			if (CaptureMode ==
			ETrackEdgeCaptureMode::Lit)
			{
				RTResource->ReadPixels(PixelData);
			}
			else
			{
				FReadSurfaceDataFlags ReadPixelFlags;
				RTResource->ReadPixels(
                				PixelData,
                				ReadPixelFlags
                			);
                			
				IConsoleManager::Get().FindConsoleVariable(
				TEXT("r.EyeAdaptationQuality"))->Set(1);
			}
			
			CaptureComp->TextureTarget = nullptr;
			
			// SAVE IMAGE
			
			const int32 SavedRow = Row;

			FString FileName =
				FString::Printf(
					TEXT("Map_%d_%d.png"),
					SavedRow,
					Col
				);
			
			FString SavePath =
				SessionFolder / FileName;

			TArray64<uint8> PNGData;

			TArray<FColor> FinalPixels;
			FinalPixels.SetNum(1024 * 1024);

			for (int32 PixelY = 0; PixelY < 1024; PixelY++)
			{
				for (int32 PixelX = 0; PixelX < 1024; PixelX++)
				{
					const int32 Source =
	             PixelY * 1024 + PixelX;
 
                      const int32 DestX =
                          PixelX;
                      
                      const int32 DestY =
                          1023 - PixelY;

					const int32 Dest =
						DestY * 1024 + DestX;

					FinalPixels[Dest] =
						PixelData[Source];
				}
			}

			PixelData = MoveTemp(FinalPixels);			
			
			FImageUtils::PNGCompressImageArray(
				1024,
				1024,
				PixelData,
				PNGData
			);

			FFileHelper::SaveArrayToFile(
				PNGData,
				*SavePath
			);

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Saved: %s"),
				*SavePath
			);

			// CLEANUP

			CaptureActor->Destroy();
			
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Grid Capture Complete"));
	return true;
}