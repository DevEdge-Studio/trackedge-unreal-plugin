// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#include "TrackEdgeSubsystem.h"
#include "TrackEdgeSettings.h"
#include "HttpModule.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Engine/LevelBounds.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformMisc.h"
#include "RHI.h"
#include "EngineUtils.h"


bool UTrackEdgeSubsystem::IsReplayEnabledForCurrentLevel() const
{
    const FTrackEdgeLevelMapping* Mapping =
        GetCurrentLevelMapping();

    if (!Mapping)
    {
        return false;
    }

    return Mapping->bEnableReplay;
}

// INIT

void UTrackEdgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadOrCreatePlayerId();
    GenerateFingerprint();
    InitSession();
    
    FCoreUObjectDelegates::PreLoadMap.AddUObject(
    this,
    &UTrackEdgeSubsystem::OnPreLoadMap
    );

    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this,
        &UTrackEdgeSubsystem::OnPostLoadMap
    );
    
}

// PLAYER ID

void UTrackEdgeSubsystem::LoadOrCreatePlayerId()
{
    FString Path = FPaths::ProjectSavedDir() + TEXT("TrackEdge_PlayerId.txt");

    if (FPaths::FileExists(Path))
    {
        FFileHelper::LoadFileToString(PlayerId, *Path);
    }
    else
    {
        PlayerId = FGuid::NewGuid().ToString();
        FFileHelper::SaveStringToFile(PlayerId, *Path);
    }

    UE_LOG(LogTemp, Warning, TEXT("PlayerId: %s"), *PlayerId);
}

// FINGERPRINT

void UTrackEdgeSubsystem::GenerateFingerprint()
{
    FString Raw = FPlatformMisc::GetDeviceId() + FPlatformMisc::GetOSVersion();
    Fingerprint = FMD5::HashAnsiString(*Raw);

    UE_LOG(LogTemp, Warning, TEXT("Fingerprint: %s"), *Fingerprint);
}

// INIT SESSION

void UTrackEdgeSubsystem::InitSession()
{
    const UTrackEdgeSettings* Settings =
    GetDefault<UTrackEdgeSettings>();

    FString Url = FString::Printf(
        TEXT("%s/api/v1/events/init?pid=%s"),
        *Settings->BaseUrl,
        *Settings->ProjectId
    );

    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("fingerprint"), Fingerprint);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(Output);

    TWeakObjectPtr<UTrackEdgeSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            
            UTrackEdgeSubsystem* Self = WeakThis.Get();

            if (Self->bSubsystemShuttingDown)
            {
                return;
            }

            if (!bSuccess || !Res.IsValid())
            {
                if
                (
                    Self->RetryRequest(
                        Self->InitRetry,
                        [Self]()
                        {
                            Self->InitSession();
                        }
                    )
                )
                {
                    return;
                }

                UE_LOG(LogTemp, Error, TEXT("Init permanently failed"));

                return;
            }

            FString Response = Res->GetContentAsString();

            UE_LOG(LogTemp, Warning, TEXT("Init Response: %s"), *Response);

            TSharedPtr<FJsonObject> JsonObj;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Response);

            if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
            {
                if (JsonObj->HasField(TEXT("data")))
                {
                    auto Data = JsonObj->GetObjectField(TEXT("data"));

                    Self->Signature =
                        Data->GetStringField(TEXT("signature"));

                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("Signature Stored: %s"),
                        *Self->Signature
                    );
                    
                    Self->ResetRetry(Self->InitRetry);
                    Self->bIsInitialized = true;

                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("Init complete → calling Identify")
                    );

                    Self->IdentifyPlayer();
                }
            }
        }
    );

    Request->ProcessRequest();
}

void UTrackEdgeSubsystem::Deinitialize()
{
    UE_LOG(LogTemp, Warning, TEXT("TrackEdge Deinitialize"));

    EndReplaySession(false);

    Super::Deinitialize();
    
    FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
}

// IDENTIFY PLAYER

void UTrackEdgeSubsystem::IdentifyPlayer()
{
    if (Signature.IsEmpty() || Fingerprint.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Identify blocked: missing headers"));
        return;
    }

    const UTrackEdgeSettings* Settings =
    GetDefault<UTrackEdgeSettings>();

    FString Url = FString::Printf(
        TEXT("%s/api/v1/events/identify?pid=%s"),
        *Settings->BaseUrl,
        *Settings->ProjectId
    );

    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("playerId"), PlayerId);

    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

    //Platform & Build Type
    
    Props->SetStringField(
        TEXT("platform"),
        FPlatformProperties::PlatformName()
    );

#if UE_BUILD_DEBUG
    Props->SetStringField(TEXT("build"), TEXT("Debug"));
#elif UE_BUILD_DEVELOPMENT
    Props->SetStringField(TEXT("build"), TEXT("Development"));
#elif UE_BUILD_TEST
    Props->SetStringField(TEXT("build"), TEXT("Test"));
#elif UE_BUILD_SHIPPING
    Props->SetStringField(TEXT("build"), TEXT("Shipping"));
#else
    Props->SetStringField(TEXT("build"), TEXT("Unknown"));
#endif
    
    Json->SetObjectField(TEXT("properties"), Props);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    Request->SetHeader(TEXT("x-opengame-fingerprint"), Fingerprint);
    Request->SetHeader(TEXT("x-opengame-signature"), Signature);

    Request->SetContentAsString(Output);

    TWeakObjectPtr<UTrackEdgeSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UTrackEdgeSubsystem* Self = WeakThis.Get();

            if (Self->bSubsystemShuttingDown)
            {
                return;
            }

            if (!bSuccess || !Res.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Identify failed"));
                return;
            }

            FString Response = Res->GetContentAsString();

            UE_LOG(
                LogTemp,
                Error,
                TEXT("IDENTIFY RAW RESPONSE = %s"),
                *Response
            );

            UE_LOG(
                LogTemp,
                Error,
                TEXT("IDENTIFY HTTP CODE = %d"),
                Res->GetResponseCode()
            );

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Identify Response: %s"),
                *Response
            );

            Self->bIsIdentified = true;

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Identify request completed successfully")
            );

            if (!Self->bHasTrackedEvent)
            {
                Self->EnsureReplayTracked();
            }
            else
            {
                Self->EndReplaySession(true);
            }
        }
    );

    Request->ProcessRequest();
}

// TRACK EVENT

void UTrackEdgeSubsystem::TrackEvent(
    const FString& EventName,
    const TMap<FString, FString>& Properties,
    TFunction<void(bool, const FString&)> Callback
)
{
    if (!bIsInitialized || !bIsIdentified)
    {
        PendingTrackEventName = EventName;
        PendingTrackProperties = Properties;
        PendingTrackCallback = Callback;
        
        UE_LOG(LogTemp, Warning, TEXT("Track blocked: Init/Identify not ready"));
        if (Callback)
        {
            Callback(false, TEXT("Not ready"));
        }
        return;
    }

    const UTrackEdgeSettings* Settings =
    GetDefault<UTrackEdgeSettings>();

    FString Url = FString::Printf(
        TEXT("%s/api/v1/events/track?pid=%s"),
        *Settings->BaseUrl,
        *Settings->ProjectId
    );

    // BASIC EVENT
TSharedPtr<FJsonObject> Event = MakeShareable(new FJsonObject());

Event->SetStringField(TEXT("name"), EventName);
Event->SetStringField(TEXT("type"), TEXT("TRACK"));
Event->SetNumberField(TEXT("value"), 1);
Event->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
Event->SetStringField(TEXT("playerId"), PlayerId);

// ================== PLATFORM ==================
Event->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
Event->SetStringField(TEXT("osVersion"), FPlatformMisc::GetOSVersion());

    // Type of platform
    
#if PLATFORM_WINDOWS
    Event->SetStringField(TEXT("device"), TEXT("Windows"));
#elif PLATFORM_LINUX
    Event->SetStringField(TEXT("device"), TEXT("Linux"));
#elif PLATFORM_MAC
    Event->SetStringField(TEXT("device"), TEXT("Mac"));
#elif PLATFORM_ANDROID
    Event->SetStringField(TEXT("device"), TEXT("Android"));
#elif PLATFORM_IOS
    Event->SetStringField(TEXT("device"), TEXT("iOS"));
#else
    Event->SetStringField(TEXT("device"), TEXT("Unknown"));
#endif
    
    // Build Type
    
#if UE_BUILD_DEBUG
    Event->SetStringField(TEXT("build"), TEXT("Debug"));
#elif UE_BUILD_DEVELOPMENT
    Event->SetStringField(TEXT("build"), TEXT("Development"));
#elif UE_BUILD_TEST
    Event->SetStringField(TEXT("build"), TEXT("Test"));
#elif UE_BUILD_SHIPPING
    Event->SetStringField(TEXT("build"), TEXT("Shipping"));
#else
    Event->SetStringField(TEXT("build"), TEXT("Unknown"));
#endif

    //Graphics API.
    
    Event->SetStringField(
    TEXT("rhi"),
    GDynamicRHI ? GDynamicRHI->GetName() : TEXT("Unknown")
);
    //Number of CPU cores.
    
    Event->SetNumberField(
    TEXT("cpuCores"),
    FPlatformMisc::NumberOfCores()
);
    
    // Current FPS
    
    float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
    
    const int32 FPS =
        DeltaSeconds > SMALL_NUMBER
            ? FMath::RoundToInt(1.f / DeltaSeconds)
            : 0;

    Event->SetNumberField(
        TEXT("fps"),
        FPS
    );
    
// ENGINE
Event->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());

// DISPLAY
UGameViewportClient* Viewport =
    GetWorld() ? GetWorld()->GetGameViewport() : nullptr;

if (Viewport && Viewport->Viewport)
{
    FIntPoint Size = Viewport->Viewport->GetSizeXY();

    Event->SetNumberField(TEXT("width"), Size.X);
    Event->SetNumberField(TEXT("height"), Size.Y);

    FString Screen =
        FString::Printf(TEXT("%dx%d"), Size.X, Size.Y);

    Event->SetStringField(TEXT("screen"), Screen);
}
    
// HARDWARE
Event->SetStringField(TEXT("cpu"), FPlatformMisc::GetCPUBrand());
Event->SetStringField(TEXT("gpu"), GRHIAdapterName);
Event->SetNumberField(TEXT("ram"), FPlatformMemory::GetPhysicalGBRam());

//USER AGENT
    Event->SetStringField(
     TEXT("userAgent"),
     FString::Printf(
         TEXT("TrackEdge UE %s"),
         *FEngineVersion::Current().ToString()
     )
 );

// SOURCE
Event->SetStringField(TEXT("source"), TEXT("client"));

// CUSTOM PROPERTIES
TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject());

for (const TPair<FString, FString>& Pair : Properties)
{
    Props->SetStringField(Pair.Key, Pair.Value);
}

Event->SetObjectField(TEXT("properties"), Props);

//FINAL WRAP 
TArray<TSharedPtr<FJsonValue>> Events;
Events.Add(MakeShareable(new FJsonValueObject(Event)));

TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
Root->SetArrayField(TEXT("e"), Events);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    UE_LOG(LogTemp, Warning, TEXT("Track Payload: %s"), *Output);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    Request->SetHeader(TEXT("x-opengame-fingerprint"), Fingerprint);
    Request->SetHeader(TEXT("x-opengame-signature"), Signature);

    Request->SetContentAsString(Output);

    TWeakObjectPtr<UTrackEdgeSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, Callback](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
        {
            if (!WeakThis.IsValid())
            {
                if (Callback)
                {
                    Callback(false, TEXT("Subsystem destroyed"));
                }

                return;
            }

            UTrackEdgeSubsystem* Self = WeakThis.Get();

            if (Self->bSubsystemShuttingDown)
            {
                if (Callback)
                {
                    Callback(false, TEXT("Subsystem shutting down"));
                }

                return;
            }

            if (!bSuccess || !Res.IsValid())
            {
                if
                (
                    Self->RetryRequest(
                        Self->TrackRetry,
                        [Self]()
                        {
                            Self->TrackEvent(
                                Self->PendingTrackEventName,
                                Self->PendingTrackProperties,
                                Self->PendingTrackCallback
                            );
                        }
                    )
                )
                {
                    return;
                }

                if (Self->PendingTrackCallback)
                {
                    Self->PendingTrackCallback(
                        false,
                        TEXT("Track Failed")
                    );
                }

                return;
            }
            
            Self->ResetRetry(Self->TrackRetry);
            Self->PendingTrackEventName.Empty();
            Self->PendingTrackProperties.Empty();
            Self->PendingTrackCallback = nullptr;
            Self->bHasTrackedEvent = true;

            if (Callback)
            {
                Callback(true, Res->GetContentAsString());
            }
        }
    );

    Request->ProcessRequest();
}

void UTrackEdgeSubsystem::StartReplaySession(
    bool bIgnoreLevelSetting,
    TFunction<void(bool,const FString&)> Callback
)
{   
    if (!bIgnoreLevelSetting &&
    !IsReplayEnabledForCurrentLevel())
    {
        UE_LOG(LogTemp, Warning, TEXT("Replay disabled for current level"));

        if (Callback)
        {
            Callback(false, TEXT("Replay disabled"));
        }

        return;
    }
    
    if (!bIsInitialized || !bIsIdentified)
    {
        return;
    }

    if (bReplayRunning)
    {
        EndReplaySession(true, Callback);
    }
    else
    {
        StartReplaySession_Internal(Callback);
    }
}

void UTrackEdgeSubsystem::EndReplaySession(
    bool bStartNew,
    TFunction<void(bool,const FString&)> Callback
)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(
            ReplayTimerHandle
        );
    }
    
    // ADD THIS HERE
    if (!bReplayRunning)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Replay already stopped")
        );

        if (bStartNew)
        {
            StartReplaySession();
        }

        return;
    }
    
    if (Signature.IsEmpty() || Fingerprint.IsEmpty())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Replay End blocked: missing headers")
        );

        return;
    }

    const UTrackEdgeSettings* Settings =
    GetDefault<UTrackEdgeSettings>();

    FString Url = FString::Printf(
        TEXT("%s/api/v1/events/replay/end?pid=%s"),
        *Settings->BaseUrl,
        *Settings->ProjectId
    );

    TSharedRef<IHttpRequest> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(Url);

    Request->SetVerb(TEXT("POST"));

    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json")
    );

    Request->SetHeader(
        TEXT("x-opengame-fingerprint"),
        Fingerprint
    );

    Request->SetHeader(
        TEXT("x-opengame-signature"),
        Signature
    );

    Request->SetContentAsString(TEXT("{}"));
    TWeakObjectPtr<UTrackEdgeSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, bStartNew, Callback]
        (
            FHttpRequestPtr Req,
            FHttpResponsePtr Res,
            bool bSuccess
        )
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UTrackEdgeSubsystem* Self = WeakThis.Get();

            if (Self->bSubsystemShuttingDown)
            {
                return;
            }

            if (!bSuccess || !Res.IsValid())
            {
                if
                (
                    Self->RetryRequest(
                        Self->ReplayEndRetry,
                        [Self, bStartNew, Callback]()
                        {
                            Self->EndReplaySession(
                                bStartNew,
                                Callback
                            );
                        }
                    )
                )
                {
                    return;
                }
            
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("Replay End Failed")
                );
            
                if (Callback)
                {
                    Callback(false, TEXT("Replay End Failed"));
                }
            
                if (bStartNew)
                {
                    Self->StartReplaySession(false, Callback);
                }
            
                return;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Replay End Success")
            );

            if (Callback)
            {
                Callback(true, Res->GetContentAsString());
            }
            
            Self->ResetRetry(Self->ReplayEndRetry);
            Self->ReplayBuffer.Empty();
            Self->ReplaySessionId.Empty();
            Self->ReplayChunkIndex = 1;
            Self->bReplayUploadInProgress = false;

            if (bStartNew)
            {
                Self->StartReplaySession_Internal(Callback);
            }
        }
    );
    
    Request->ProcessRequest();
    bReplayRunning = false;
}

void UTrackEdgeSubsystem::StartReplaySession_Internal(
    TFunction<void(bool,const FString&)> Callback
)
{
    if (bReplayRunning)
    {
        return;
    }
    
    const UTrackEdgeSettings* Settings =
    GetDefault<UTrackEdgeSettings>();
    
    const FTrackEdgeLevelMapping* Mapping =
    GetCurrentLevelMapping();

    if (!Mapping)
    {
        UE_LOG(LogTemp, Error, TEXT("No level mapping found"));
        if (Callback)
        {
            Callback(false, TEXT("No Level Mapping"));
        }
        return;
    }

    FString Url = FString::Printf(
        TEXT("%s/api/v1/events/replay/start?pid=%s"),
        *Settings->BaseUrl,
        *Settings->ProjectId
    );

    TSharedPtr<FJsonObject> Json =
        MakeShared<FJsonObject>();
    
    FString CurrentLevel =
    UGameplayStatics::GetCurrentLevelName(
        GetWorld(),
        true
    );

    Json->SetStringField(
        TEXT("name"),
        CurrentLevel
    );

    TSharedPtr<FJsonObject> Metadata =
        MakeShared<FJsonObject>();
    
    UE_LOG(
    LogTemp,
    Warning,
    TEXT("Replay Name = %s"),
    *CurrentLevel
);

    Metadata->SetStringField(
        TEXT("source"),
        TEXT("unreal")
    );

    Json->SetObjectField(
        TEXT("metadata"),
        Metadata
    );

    FString Output;

    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Output);

    FJsonSerializer::Serialize(
        Json.ToSharedRef(),
        Writer
    );

    TSharedRef<IHttpRequest> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(Url);

    Request->SetVerb(TEXT("POST"));

    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json")
    );

    Request->SetHeader(
        TEXT("x-opengame-fingerprint"),
        Fingerprint
    );

    Request->SetHeader(
        TEXT("x-opengame-signature"),
        Signature
    );

    Request->SetContentAsString(Output);
    TWeakObjectPtr<UTrackEdgeSubsystem> WeakThis(this);
    
    UE_LOG(
    LogTemp,
    Warning,
    TEXT("Replay Fingerprint = %s"),
    *Fingerprint
);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Replay Signature = %s"),
        *Signature
    );

    Request->OnProcessRequestComplete().BindLambda(
    [WeakThis, Mapping, Callback]
        (
            FHttpRequestPtr Req,
            FHttpResponsePtr Res,
            bool bSuccess
        )
        {   
        
        if (!WeakThis.IsValid())
        {
            return;
        }

        UTrackEdgeSubsystem* Self = WeakThis.Get();

        if (Self->bSubsystemShuttingDown)
        {
            return;
        }
        
        if (!bSuccess || !Res.IsValid())
        {
            if
            (
              Self->RetryRequest(
           Self->ReplayStartRetry,
            [Self, Callback]()
        {
            Self->StartReplaySession_Internal(
                Callback
            );
        }
    )
)
{
    return;
}

            if (Callback)
            {
                Callback(false, TEXT("Replay Start Failed"));
            }

            return;
        }

            FString Response =
                Res->GetContentAsString();

            UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("FULL REPLAY RESPONSE = %s"),
                    *Response
                   );

                   UE_LOG(
                        LogTemp,
                        Error,
                        TEXT("HTTP CODE = %d"),
                        Res->GetResponseCode()
                   );
            
                   UE_LOG(
                          LogTemp,
                          Error,
                          TEXT("START REPLAY RAW RESPONSE = %s"),
                          *Response
                          );

                   UE_LOG(
                          LogTemp,
                          Error,
                          TEXT("START REPLAY HTTP CODE = %d"),
                          Res->GetResponseCode()
                          );

            TSharedPtr<FJsonObject> JsonObj;

            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(
                    Response
                );

            if (
                FJsonSerializer::Deserialize(
                    Reader,
                    JsonObj
                )
                &&
                JsonObj.IsValid()
            )
            {
                Self->ReplaySessionId =
                JsonObj->GetStringField(
                TEXT("id")
                 );

                if (Self->GetWorld())
                {
                    const UTrackEdgeSettings* Settings =
                    GetDefault<UTrackEdgeSettings>();
                    
                    Self->GetWorld()->GetTimerManager().SetTimer(
                    Self->ReplayTimerHandle,
                             Self,
                         &UTrackEdgeSubsystem::RecordReplayPoint,
                         Mapping->ReplaySampleInterval,
                       true
                       );
                }
                
                Self->bReplayRunning = true;
                Self->ReplayChunkIndex = 1;
                Self->CacheLevelBounds();
                
                Self->ResetRetry(Self->ReplayStartRetry);
                
                if (Callback)
                {
                    Callback(true, Response);
                }

                UE_LOG(
                LogTemp,
                Warning,
                TEXT("ReplaySessionId: %s"),
                *Self->ReplaySessionId
               );
            }
        }
    );

    Request->ProcessRequest();
}

void UTrackEdgeSubsystem::RecordReplayPoint()
{
    if (!bReplayRunning)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    APawn* PlayerPawn =
        World->GetFirstPlayerController()
        ? World->GetFirstPlayerController()->GetPawn()
        : nullptr;

    if (!PlayerPawn)
    {
        return;
    }

    FVector Location =
        PlayerPawn->GetActorLocation();

    FRotator Rotation =
        World->GetFirstPlayerController()
        ->GetControlRotation();

    FTrackEdgeReplayPoint Point;

    Point.Time =
        FDateTime::UtcNow().ToIso8601();

    Point.X = Location.X;
    Point.Y = Location.Y;
    Point.Z = Location.Z;

    Point.Yaw = Rotation.Yaw;
    Point.Pitch = Rotation.Pitch;
    
    // FPS
    float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
    
    const int32 FPS =
        DeltaSeconds > SMALL_NUMBER
            ? FMath::RoundToInt(1.f / DeltaSeconds)
            : 0;
    Point.FPS = FPS;
    
    // RAM
    const FPlatformMemoryStats Memory =
    FPlatformMemory::GetStats();

    //RAM
    Point.Ram =
        Memory.UsedPhysical / (1024.f * 1024.f);
    
    //GPU
    Point.Gpu = -1.f;
    
    //FPS Drop
    Point.FPSDrop =
    Point.FPS < 30 ? 1 : 0;
    
    const float Width  = MapMaxX - MapMinX;
    const float Height = MapMaxY - MapMinY;
    const float Depth  = MapMaxZ - MapMinZ;
    
    if (Width > KINDA_SMALL_NUMBER)
    {
        Point.NormalizedX = FMath::Clamp(
            (Location.X - MapMinX) / Width,
            0.f,
            1.f
        );
    }
    else
    {
        Point.NormalizedX = 0.f;
    }

    if (Height > KINDA_SMALL_NUMBER)
    {
        Point.NormalizedY = FMath::Clamp(
            (Location.Y - MapMinY) / Height,
            0.f,
            1.f
        );
    }
    else
    {
        Point.NormalizedY = 0.f;
    }
    
    if (Depth > KINDA_SMALL_NUMBER)
    {
        Point.NormalizedZ = FMath::Clamp(
            (Location.Z - MapMinZ) / Depth,
            0.f,
            1.f
        );
    }
    else
    {
        Point.NormalizedZ = 0.f;
    }
    
    ReplayBuffer.Add(Point);
    
    const FTrackEdgeLevelMapping* Mapping =
 GetCurrentLevelMapping();

    if (!Mapping)
    {
        return;
    }

    if (ReplayBuffer.Num() >= Mapping->ReplayChunkSize)
    {
        UploadReplayChunk();
    }
    
#if WITH_EDITOR
    if (ReplayBuffer.Num() % 25 == 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Replay Buffer Size: %d"),
            ReplayBuffer.Num()
        );
    }
#endif
}
void UTrackEdgeSubsystem::CacheLevelBounds()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    ALevelBounds* LevelBounds = nullptr;

    for (TActorIterator<ALevelBounds> It(World); It; ++It)
    {
        LevelBounds = *It;
        break;
    }

    if (!LevelBounds)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("TrackEdge: No LevelBounds actor found")
        );

        return;
    }

    FVector Origin;
    FVector Extent;

    LevelBounds->GetActorBounds(
        false,
        Origin,
        Extent
    );

    FVector Min = Origin - Extent;
    FVector Max = Origin + Extent;

    MapMinX = Min.X;
    MapMinY = Min.Y;
    MapMinZ = Min.Z;

    MapMaxX = Max.X;
    MapMaxY = Max.Y;
    MapMaxZ = Max.Z;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("TrackEdge Bounds Cached")
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Min=(%.2f, %.2f, %.2f)"),
        MapMinX,
        MapMinY,
        MapMinZ
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Max=(%.2f, %.2f, %.2f)"),
        MapMaxX,
        MapMaxY,
        MapMaxZ
    );
}

static FString ReplayEventTypeToString(
    ETrackEdgeReplayEventType Type)
{
    switch (Type)
    {
    case ETrackEdgeReplayEventType::PLAYER_START:
        return TEXT("PLAYER_START");

    case ETrackEdgeReplayEventType::PLAYER_DEATH:
        return TEXT("PLAYER_DEATH");

    case ETrackEdgeReplayEventType::PLAYER_LEAVE:
        return TEXT("PLAYER_LEAVE");

    case ETrackEdgeReplayEventType::CUSTOM_EVENT:
        return TEXT("CUSTOM_EVENT");

    default:
        return TEXT("SESSION_REPLAY");
    }
}

void UTrackEdgeSubsystem::UploadReplayChunk()
{
    if (!bReplayRunning)
    {
        return;
    }

    if (bReplayUploadInProgress)
    {
        return;
    }
    
    if (ReplayBuffer.Num() == 0)
    {
        return;
    }
    
    TArray<FTrackEdgeReplayPoint> ChunkToUpload = ReplayBuffer;
    
    if (ReplaySessionId.IsEmpty())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("UploadReplayChunk: Missing ReplaySessionId")
        );

        return;
    }

    const UTrackEdgeSettings* Settings =
        GetDefault<UTrackEdgeSettings>();

    FString Url = FString::Printf(
        TEXT("%s/api/v1/events/replay?pid=%s"),
        *Settings->BaseUrl,
        *Settings->ProjectId
    );

    TSharedPtr<FJsonObject> Root =
        MakeShared<FJsonObject>();

    Root->SetStringField(
    TEXT("eventType"),
    ReplayEventTypeToString(RecordingChunkType)
    );

    const FTrackEdgeLevelMapping* Mapping =
GetCurrentLevelMapping();

    if (!Mapping)
    {
        return;
    }

    Root->SetStringField(
        TEXT("mapSetId"),
        Mapping->MapSetId
    );
    
    Root->SetStringField(
        TEXT("replaySessionId"),
        ReplaySessionId
    );

    Root->SetNumberField(
        TEXT("chunkIndex"),
        ReplayChunkIndex
    );

    Root->SetStringField(
    TEXT("createdAt"),
    ChunkToUpload[0].Time
    );

    Root->SetStringField(
        TEXT("endedAt"),
        ChunkToUpload.Last().Time
    );

    TArray<TSharedPtr<FJsonValue>> PointsArray;

    for (const FTrackEdgeReplayPoint& Point : ChunkToUpload)
    
    {
        TSharedPtr<FJsonObject> PointObj =
            MakeShared<FJsonObject>();

        PointObj->SetStringField(
            TEXT("time"),
            Point.Time
        );
        
        TSharedPtr<FJsonObject> PositionObj =
            MakeShared<FJsonObject>();

        if (Mapping)
        {
            PointObj->SetStringField(
                TEXT("mapId"),
                Mapping->MapId
            );

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Replay Point MapId = %s"),
                *Mapping->MapId
            );
        }
        
        PositionObj->SetNumberField(
            TEXT("x"),
            Point.NormalizedX
        );

        PositionObj->SetNumberField(
            TEXT("y"),
            Point.NormalizedY
        );

        PositionObj->SetNumberField(
            TEXT("z"),
            Point.NormalizedZ
        );

        PointObj->SetObjectField(
            TEXT("position"),
            PositionObj
        );

        TSharedPtr<FJsonObject> RotationObj =
            MakeShared<FJsonObject>();

        RotationObj->SetNumberField(
            TEXT("yaw"),
            Point.Yaw
        );

        RotationObj->SetNumberField(
            TEXT("pitch"),
            Point.Pitch
        );

        PointObj->SetObjectField(
            TEXT("rotation"),
            RotationObj
        );

        TSharedPtr<FJsonObject> Computational =
    MakeShared<FJsonObject>();

        Computational->SetNumberField(
            TEXT("ram"),
            Point.Ram
        );
        
        Computational->SetNumberField(
            TEXT("fps"),
            Point.FPS
        );

        Computational->SetNumberField(
            TEXT("fpsDrop"),
            Point.FPSDrop
        );

        PointObj->SetObjectField(
            TEXT("computational"),
            Computational
        );
        
        PointsArray.Add(
            MakeShared<FJsonValueObject>(
                PointObj
            )
        );
    }

    Root->SetArrayField(
        TEXT("points"),
        PointsArray
    );

    FString Payload;

    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(
            &Payload
        );

    FJsonSerializer::Serialize(
        Root.ToSharedRef(),
        Writer
    );

    TSharedRef<IHttpRequest> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(Url);

    Request->SetVerb(TEXT("POST"));

    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json")
    );

    Request->SetHeader(
        TEXT("x-opengame-fingerprint"),
        Fingerprint
    );

    Request->SetHeader(
        TEXT("x-opengame-signature"),
        Signature
    );

    Request->SetContentAsString(
        Payload
    );
    
    TWeakObjectPtr<UTrackEdgeSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis]
        (
            FHttpRequestPtr Req,
            FHttpResponsePtr Res,
            bool bSuccess
        )
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UTrackEdgeSubsystem* Self = WeakThis.Get();

            if (Self->bSubsystemShuttingDown)
            {
                return;
            }

            if (!bSuccess || !Res.IsValid())
            {
                Self->bReplayUploadInProgress = false;

                if
                (
                    Self->RetryRequest(
                        Self->ReplayChunkRetry,
                        [Self]()
                        {
                            Self->UploadReplayChunk();
                        }
                    )
                )
                {
                    return;
                }

                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("Replay Chunk Upload Failed")
                );

                return;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Replay Chunk Upload Success")
            );

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Chunk Index = %d"),
                Self->ReplayChunkIndex
            );

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Response = %s"),
                *Res->GetContentAsString()
            );

            
            Self->bReplayUploadInProgress = false;
            Self->ResetRetry(Self->ReplayChunkRetry);
            Self->ReplayBuffer.Empty();
            Self->ReplayChunkIndex++;

            // The next chunk starts with the current event type.
            Self->RecordingChunkType = Self->CurrentReplayEventType;

            // PLAYER_START should only happen once.
            if (Self->RecordingChunkType ==
            ETrackEdgeReplayEventType::PLAYER_START)
            {
             Self->CurrentReplayEventType =
             ETrackEdgeReplayEventType::SESSION_REPLAY;

             Self->RecordingChunkType =
             ETrackEdgeReplayEventType::SESSION_REPLAY;
            }

            // Auto-reset one-shot events.
            if (Self->CurrentReplayEventType ==
            ETrackEdgeReplayEventType::PLAYER_DEATH ||
            Self->CurrentReplayEventType ==
            ETrackEdgeReplayEventType::PLAYER_LEAVE)
            {
              Self->CurrentReplayEventType =
              ETrackEdgeReplayEventType::SESSION_REPLAY;
            }
        }
    );

    UE_LOG(
    LogTemp,
    Warning,
    TEXT("Uploading Replay Chunk %d (%d points)"),
    ReplayChunkIndex,
    ReplayBuffer.Num()
);
    bReplayUploadInProgress = true;
    Request->ProcessRequest();
}

const FTrackEdgeLevelMapping*
UTrackEdgeSubsystem::GetCurrentLevelMapping() const
{
    const UTrackEdgeSettings* Settings =
        GetDefault<UTrackEdgeSettings>();
    

    FString CurrentLevel =
        UGameplayStatics::GetCurrentLevelName(
            GetWorld(),
            true
        );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Current Level = %s"),
        *CurrentLevel
    );

    for (const FTrackEdgeLevelMapping& Mapping :
         Settings->LevelMappings)
    {
        if (Mapping.LevelName == CurrentLevel)
        {
            UE_LOG(
    LogTemp,
    Warning,
    TEXT("Matched Mapping -> MapId=%s MapSetId=%s"),
    *Mapping.MapId,
    *Mapping.MapSetId
);

            return &Mapping;
        }
    }

    return nullptr;
}

void UTrackEdgeSubsystem::EnsureReplayTracked()
{
    TMap<FString, FString> DummyProps;
    TWeakObjectPtr<UTrackEdgeSubsystem> WeakThis(this);
    TrackEvent(
        TEXT("TEReplay"),
        DummyProps,
        [WeakThis](bool bSuccess, const FString&)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UTrackEdgeSubsystem* Self = WeakThis.Get();
            if (bSuccess)
            {
                Self->ResetRetry(Self->ReplayTrackRetry);
                Self->EndReplaySession(true);

                return;
            }

            if
            (
                     Self->RetryRequest(
                     Self->ReplayTrackRetry,
                     [Self]()
                                   {
                                   Self->EnsureReplayTracked();
                                   },
                    1.f
                )
            )
            {
                return;
            }

            UE_LOG(
                LogTemp,
                Error,
                TEXT("Replay Track failed after retries.")
            );
        }
    );
}

bool UTrackEdgeSubsystem::RetryRequest(
    FTrackEdgeRetryState& Retry,
    TFunction<void()> RetryFunction,
    float Delay
)
{
    Retry.RetryCount++;
    
    if (Retry.RetryCount > MaxRetries)
    {
        Retry.RetryCount = 0;
        return false;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Retry %d/%d"),
        Retry.RetryCount,
        MaxRetries
    );

    UWorld* World = GetWorld();

    if (!World)
    {
        return false;
    }

    World->GetTimerManager().SetTimer(
        Retry.Timer,
        MoveTemp(RetryFunction),
        Delay,
        false
    );

    return true;
}

void UTrackEdgeSubsystem::ResetRetry(
    FTrackEdgeRetryState& Retry
)
{
    Retry.RetryCount = 0;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(
            Retry.Timer
        );
    }
}

void UTrackEdgeSubsystem::OnPreLoadMap(
    const FString& MapName
)
{
    if (!IsReplayEnabledForCurrentLevel())
    {
        return;
    }

    if (!bReplayRunning)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("TrackEdge: Level changing -> Ending replay")
    );

    EndReplaySession(false);
}

void UTrackEdgeSubsystem::OnPostLoadMap(
    UWorld* LoadedWorld
)
{
    if (!LoadedWorld)
    {
        return;
    }

    if (!IsReplayEnabledForCurrentLevel())
    {
        return;
    }

    CurrentReplayEventType =
    ETrackEdgeReplayEventType::PLAYER_START;

    RecordingChunkType =
        ETrackEdgeReplayEventType::PLAYER_START;
    
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("TrackEdge: New level loaded -> Starting replay")
    );

    StartReplaySession();
}

void UTrackEdgeSubsystem::SetReplayEventType(
    ETrackEdgeReplayEventType NewType)
{
    if (CurrentReplayEventType == NewType)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Replay Event Changed -> %s"),
        *ReplayEventTypeToString(NewType)
    );

    // Set the event type FIRST.
    CurrentReplayEventType = NewType;

    // Then flush the current chunk.
    if (ReplayBuffer.Num() > 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Flushing current chunk because replay event changed.")
        );

        UploadReplayChunk();
    }
}