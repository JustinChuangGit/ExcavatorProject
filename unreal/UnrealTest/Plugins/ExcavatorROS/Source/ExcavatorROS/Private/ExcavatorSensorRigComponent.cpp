#include "ExcavatorSensorRigComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ExcavatorROSBridgeComponent.h"
#include "GameFramework/Actor.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogExcavatorSensors, Log, All);

namespace ExcavatorSensorBones
{
const FName Body = TEXT("B_ConstractionExcavator01_Body");
const FName Stick = TEXT("B_ConstractionExcavator01_Arm04");
const FName Bucket = TEXT("B_ConstractionExcavator01_End");
}

UExcavatorSensorRigComponent::UExcavatorSensorRigComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UExcavatorSensorRigComponent::BeginPlay()
{
    Super::BeginPlay();
    ResolveComponents();
    InitializeCameraViews();
    InitializeCameraStream();
}

void UExcavatorSensorRigComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ResolveComponents();
    if (ViewCameras.IsEmpty())
    {
        InitializeCameraViews();
    }
    if (!StreamCapture.IsValid() || !StreamRenderTarget.IsValid())
    {
        InitializeCameraStream();
    }
    UpdateCameraTransforms();
    UpdateCameraInput();

    if (ROSBridge.IsValid())
    {
        const int32 RequestedView = ROSBridge->GetRequestedCameraView();
        if (
            RequestedView != LastROSRequestedView
            && ViewCameras.IsValidIndex(RequestedView)
        )
        {
            LastROSRequestedView = RequestedView;
            if (!bOperatorMode)
            {
                SetActiveCameraView(RequestedView);
            }
        }
    }

    UWorld* World = GetWorld();
    if (
        bEnableLidar
        && World
        && LidarRateHz > 0.0f
        && World->GetTimeSeconds() >= NextLidarTimeSeconds
    )
    {
        ScanLidar();
        NextLidarTimeSeconds =
            World->GetTimeSeconds() + 1.0 / LidarRateHz;
    }
    if (
        bEnableCameraStream
        && World
        && ROSBridge.IsValid()
        && ROSBridge->IsConnected()
        && CameraStreamRateHz > 0.0f
        && World->GetTimeSeconds() >= NextCameraStreamTimeSeconds
    )
    {
        CaptureCameraStreamFrame();
        NextCameraStreamTimeSeconds =
            World->GetTimeSeconds() + 1.0 / CameraStreamRateHz;
    }
}

void UExcavatorSensorRigComponent::ResolveComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    if (!ExcavatorMesh.IsValid())
    {
        ExcavatorMesh =
            Owner->FindComponentByClass<USkeletalMeshComponent>();
    }
    if (!ROSBridge.IsValid())
    {
        ROSBridge =
            Owner->FindComponentByClass<UExcavatorROSBridgeComponent>();
    }
}

void UExcavatorSensorRigComponent::InitializeCameraViews()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->GetRootComponent())
    {
        return;
    }

    TArray<UCameraComponent*> ExistingCameras;
    Owner->GetComponents(ExistingCameras);
    if (ExistingCameras.IsEmpty())
    {
        return;
    }

    UCameraComponent* ChaseCamera = ExistingCameras[0];
    for (UCameraComponent* Existing : ExistingCameras)
    {
        if (Existing && Existing->IsActive())
        {
            ChaseCamera = Existing;
            break;
        }
    }

    ViewCameras.Reset();
    ViewNames = {
        TEXT("chase"),
        TEXT("cab"),
        TEXT("boom"),
        TEXT("bucket"),
        TEXT("rear"),
        TEXT("operator")
    };
    ViewCameras.Add(ChaseCamera);

    for (int32 Index = 1; Index < ViewNames.Num(); ++Index)
    {
        const FName ComponentName(
            *FString::Printf(TEXT("SensorCamera_%s"), *ViewNames[Index])
        );
        UCameraComponent* Camera =
            NewObject<UCameraComponent>(Owner, ComponentName);
        Owner->AddInstanceComponent(Camera);
        Camera->RegisterComponent();
        Camera->AttachToComponent(
            Owner->GetRootComponent(),
            FAttachmentTransformRules::KeepWorldTransform
        );
        Camera->SetFieldOfView(Index == 1 ? 82.0f : 90.0f);
        Camera->SetActive(false);
        ViewCameras.Add(Camera);
    }

    SetActiveCameraView(0);
    UE_LOG(
        LogExcavatorSensors,
        Log,
        TEXT("Initialized %d selectable excavator camera views"),
        ViewCameras.Num()
    );
}

void UExcavatorSensorRigComponent::InitializeCameraStream()
{
    AActor* Owner = GetOwner();
    if (
        !Owner
        || !Owner->GetRootComponent()
        || StreamCapture.IsValid()
        || StreamRenderTarget.IsValid()
    )
    {
        return;
    }

    UTextureRenderTarget2D* RenderTarget =
        NewObject<UTextureRenderTarget2D>(
            this,
            TEXT("MissionControlRenderTarget")
        );
    RenderTarget->InitCustomFormat(
        FMath::Clamp(CameraStreamWidth, 160, 1920),
        FMath::Clamp(CameraStreamHeight, 90, 1080),
        PF_B8G8R8A8,
        false
    );
    RenderTarget->TargetGamma = 2.2f;
    RenderTarget->ClearColor = FLinearColor::Black;
    RenderTarget->UpdateResourceImmediate(true);

    USceneCaptureComponent2D* Capture =
        NewObject<USceneCaptureComponent2D>(
            Owner,
            TEXT("MissionControlCameraCapture")
        );
    Owner->AddInstanceComponent(Capture);
    Capture->RegisterComponent();
    Capture->AttachToComponent(
        Owner->GetRootComponent(),
        FAttachmentTransformRules::KeepWorldTransform
    );
    Capture->TextureTarget = RenderTarget;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    // Preserve render history between manual captures so temporal
    // anti-aliasing has stable history instead of restarting every frame.
    Capture->bAlwaysPersistRenderingState = true;

    StreamRenderTarget = RenderTarget;
    StreamCapture = Capture;
    UE_LOG(
        LogExcavatorSensors,
        Log,
        TEXT("Mission Control camera stream initialized at %dx%d @ %.1f Hz"),
        RenderTarget->SizeX,
        RenderTarget->SizeY,
        CameraStreamRateHz
    );
}

void UExcavatorSensorRigComponent::CaptureCameraStreamFrame()
{
    if (
        !ROSBridge.IsValid()
        || !StreamCapture.IsValid()
        || !StreamRenderTarget.IsValid()
        || !ViewCameras.IsValidIndex(ActiveViewIndex)
        || !ViewCameras[ActiveViewIndex].IsValid()
    )
    {
        return;
    }

    UCameraComponent* SourceCamera = ViewCameras[ActiveViewIndex].Get();
    USceneCaptureComponent2D* Capture = StreamCapture.Get();
    UTextureRenderTarget2D* RenderTarget = StreamRenderTarget.Get();
    Capture->SetWorldLocationAndRotation(
        SourceCamera->GetComponentLocation(),
        SourceCamera->GetComponentRotation()
    );
    Capture->FOVAngle = SourceCamera->FieldOfView;
    Capture->PostProcessSettings = SourceCamera->PostProcessSettings;
    Capture->PostProcessBlendWeight =
        SourceCamera->PostProcessBlendWeight;
    Capture->CaptureScene();

    FTextureRenderTargetResource* Resource =
        RenderTarget->GameThread_GetRenderTargetResource();
    TArray<FColor> Pixels;
    FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
    // JPEGs are sRGB. Applying the display transfer here makes the browser
    // stream match Unreal's viewport instead of encoding linear pixels as a
    // dark, low-saturation image.
    ReadFlags.SetLinearToGamma(true);
    if (!Resource || !Resource->ReadPixels(Pixels, ReadFlags))
    {
        return;
    }

    IImageWrapperModule& ImageWrapperModule =
        FModuleManager::LoadModuleChecked<IImageWrapperModule>(
            TEXT("ImageWrapper")
        );
    const TSharedPtr<IImageWrapper> ImageWrapper =
        ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    if (
        !ImageWrapper.IsValid()
        || !ImageWrapper->SetRaw(
            Pixels.GetData(),
            static_cast<int64>(Pixels.Num()) * sizeof(FColor),
            RenderTarget->SizeX,
            RenderTarget->SizeY,
            ERGBFormat::BGRA,
            8
        )
    )
    {
        return;
    }

    const TArray64<uint8>& Compressed =
        ImageWrapper->GetCompressed(
            FMath::Clamp(CameraStreamJpegQuality, 10, 100)
        );
    if (!Compressed.IsEmpty())
    {
        const FString FrameId =
            ViewNames.IsValidIndex(ActiveViewIndex)
                ? FString::Printf(
                    TEXT("%s_camera_link"),
                    *ViewNames[ActiveViewIndex]
                )
                : TEXT("camera_link");
        ROSBridge->PublishCompressedCameraImage(
            Compressed.GetData(),
            static_cast<uint32>(Compressed.Num()),
            TEXT("jpeg"),
            FrameId
        );
    }
}

void UExcavatorSensorRigComponent::SetActiveCameraView(
    const int32 ViewIndex
)
{
    const int32 OperatorViewIndex = ViewNames.IndexOfByKey(
        TEXT("operator")
    );
    if (
        !ViewCameras.IsValidIndex(ViewIndex)
        || (
            bOperatorMode
            && OperatorViewIndex != INDEX_NONE
            && ViewIndex != OperatorViewIndex
        )
    )
    {
        return;
    }

    for (int32 Index = 0; Index < ViewCameras.Num(); ++Index)
    {
        if (ViewCameras[Index].IsValid())
        {
            ViewCameras[Index]->SetActive(Index == ViewIndex);
        }
    }
    ActiveViewIndex = ViewIndex;
    if (ROSBridge.IsValid() && ViewNames.IsValidIndex(ViewIndex))
    {
        ROSBridge->PublishActiveCamera(
            ViewIndex,
            ViewNames[ViewIndex]
        );
    }
    UE_LOG(
        LogExcavatorSensors,
        Log,
        TEXT("Camera view %d: %s"),
        ActiveViewIndex,
        ViewNames.IsValidIndex(ActiveViewIndex)
            ? *ViewNames[ActiveViewIndex]
            : TEXT("unknown")
    );
}

void UExcavatorSensorRigComponent::SetOperatorMode(
    const bool bOnFoot
)
{
    if (bOperatorMode == bOnFoot)
    {
        return;
    }

    const int32 OperatorViewIndex = ViewNames.IndexOfByKey(
        TEXT("operator")
    );
    if (bOnFoot)
    {
        if (ActiveViewIndex != OperatorViewIndex)
        {
            PreviousVehicleViewIndex = FMath::Clamp(
                ActiveViewIndex,
                0,
                FMath::Max(OperatorViewIndex - 1, 0)
            );
        }
        bOperatorMode = true;
        SetActiveCameraView(OperatorViewIndex);
    }
    else
    {
        bOperatorMode = false;
        SetActiveCameraView(PreviousVehicleViewIndex);
    }
}

void UExcavatorSensorRigComponent::CycleCameraView(const int32 Direction)
{
    if (ViewCameras.IsEmpty() || bOperatorMode)
    {
        return;
    }
    const int32 Count = ViewCameras.Num();
    const int32 Next =
        (ActiveViewIndex + (Direction >= 0 ? 1 : Count - 1)) % Count;
    SetActiveCameraView(Next);
}

void UExcavatorSensorRigComponent::UpdateCameraInput()
{
    if (bOperatorMode)
    {
        bCycleForwardWasDown = false;
        bCycleBackwardWasDown = false;
        return;
    }

    UWorld* World = GetWorld();
    APlayerController* Controller =
        World ? World->GetFirstPlayerController() : nullptr;
    if (!Controller)
    {
        return;
    }

    const bool bForwardDown =
        Controller->IsInputKeyDown(EKeys::Gamepad_DPad_Right)
        || Controller->IsInputKeyDown(EKeys::C);
    const bool bBackwardDown =
        Controller->IsInputKeyDown(EKeys::Gamepad_DPad_Left);

    if (bForwardDown && !bCycleForwardWasDown)
    {
        CycleCameraView(1);
    }
    if (bBackwardDown && !bCycleBackwardWasDown)
    {
        CycleCameraView(-1);
    }
    bCycleForwardWasDown = bForwardDown;
    bCycleBackwardWasDown = bBackwardDown;
}

FTransform UExcavatorSensorRigComponent::GetBoneTransform(
    const FName BoneName
) const
{
    if (!ExcavatorMesh.IsValid())
    {
        return GetOwner()
            ? GetOwner()->GetActorTransform()
            : FTransform::Identity;
    }
    return ExcavatorMesh->GetSocketTransform(BoneName, RTS_World);
}

void UExcavatorSensorRigComponent::UpdateCameraTransforms()
{
    AActor* Owner = GetOwner();
    if (
        !Owner
        || ViewCameras.Num() < 6
        || !ExcavatorMesh.IsValid()
    )
    {
        return;
    }

    const FTransform Machine = Owner->GetActorTransform();
    const FTransform Body = GetBoneTransform(ExcavatorSensorBones::Body);
    const FTransform Stick = GetBoneTransform(ExcavatorSensorBones::Stick);
    const FTransform Bucket = GetBoneTransform(ExcavatorSensorBones::Bucket);
    const FVector Up = FVector::UpVector;
    const FVector MachineForward =
        -Machine.GetUnitAxis(EAxis::X).GetSafeNormal();
    const FVector BodyForward =
        -Body.GetUnitAxis(EAxis::X).GetSafeNormal();
    const FVector BodyRight =
        Body.GetUnitAxis(EAxis::Y).GetSafeNormal();
    const FVector BucketLocation = Bucket.GetLocation();

    if (ViewCameras[1].IsValid())
    {
        const FVector Location =
            Body.GetLocation()
            + Up * 145.0f
            + BodyForward * 28.0f
            - BodyRight * 26.0f;
        ViewCameras[1]->SetWorldLocationAndRotation(
            Location,
            BodyForward.Rotation()
        );
    }

    if (ViewCameras[2].IsValid())
    {
        const FVector Location =
            Body.GetLocation()
            + Up * 220.0f
            - BodyRight * 55.0f;
        ViewCameras[2]->SetWorldLocationAndRotation(
            Location,
            (BucketLocation - Location).Rotation()
        );
    }

    if (ViewCameras[3].IsValid())
    {
        const FVector Location =
            Stick.GetLocation()
            + Up * 38.0f
            - BodyRight * 28.0f;
        ViewCameras[3]->SetWorldLocationAndRotation(
            Location,
            (BucketLocation - Location).Rotation()
        );
    }

    if (ViewCameras[4].IsValid())
    {
        const FVector Target =
            Owner->GetActorLocation()
            + Up * 95.0f;
        const FVector Location =
            Owner->GetActorLocation()
            - MachineForward * 430.0f
            + Up * 220.0f;
        ViewCameras[4]->SetWorldLocationAndRotation(
            Location,
            (Target - Location).Rotation()
        );
    }

    if (ViewCameras[5].IsValid())
    {
        UWorld* World = GetWorld();
        APlayerController* Controller =
            World ? World->GetFirstPlayerController() : nullptr;
        if (Controller)
        {
            FVector ViewLocation = FVector::ZeroVector;
            FRotator ViewRotation = FRotator::ZeroRotator;
            Controller->GetPlayerViewPoint(
                ViewLocation,
                ViewRotation
            );
            ViewCameras[5]->SetWorldLocationAndRotation(
                ViewLocation,
                ViewRotation
            );
            if (Controller->PlayerCameraManager)
            {
                ViewCameras[5]->SetFieldOfView(
                    Controller->PlayerCameraManager->GetFOVAngle()
                );
            }
        }
    }
}

void UExcavatorSensorRigComponent::ScanLidar()
{
    if (!ExcavatorMesh.IsValid() || !ROSBridge.IsValid())
    {
        return;
    }

    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World)
    {
        return;
    }

    FTransform LidarTransform;
    if (!GetLidarWorldTransform(LidarTransform))
    {
        return;
    }

    const FVector Up = FVector::UpVector;
    const FVector Origin = LidarTransform.GetLocation();
    const FVector Forward = LidarTransform.GetUnitAxis(EAxis::X);
    const int32 RayCount = FMath::Clamp(LidarRayCount, 36, 1440);
    const float MaximumRangeCentimeters =
        FMath::Max(LidarMaximumRangeMeters, 1.0f) * 100.0f;

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(ExcavatorLidar),
        true,
        Owner
    );
    QueryParams.bTraceComplex = true;
    TArray<float> Ranges;
    Ranges.Reserve(RayCount);

    for (int32 Index = 0; Index < RayCount; ++Index)
    {
        const float Alpha =
            static_cast<float>(Index) / static_cast<float>(RayCount);
        const float AngleDegrees = -180.0f + Alpha * 360.0f;
        const FVector Direction =
            Forward.RotateAngleAxis(-AngleDegrees, Up);
        const FVector End =
            Origin + Direction * MaximumRangeCentimeters;
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(
            Hit,
            Origin,
            End,
            ECC_Visibility,
            QueryParams
        );
        Ranges.Add(
            bHit
                ? Hit.Distance / 100.0f
                : LidarMaximumRangeMeters + 1.0f
        );
    }

    ROSBridge->PublishLaserScan(
        Ranges,
        -PI,
        PI,
        0.25f,
        LidarMaximumRangeMeters,
        1.0f / FMath::Max(LidarRateHz, 1.0f),
        TEXT("lidar_link")
    );
}

bool UExcavatorSensorRigComponent::GetLidarWorldTransform(
    FTransform& OutTransform
) const
{
    if (!ExcavatorMesh.IsValid())
    {
        return false;
    }

    const FTransform Body = GetBoneTransform(ExcavatorSensorBones::Body);
    const FVector Up = FVector::UpVector;
    FVector Forward = FVector::VectorPlaneProject(
        -Body.GetUnitAxis(EAxis::X),
        Up
    ).GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
        const AActor* Owner = GetOwner();
        Forward = Owner
            ? FVector::VectorPlaneProject(
                Owner->GetActorForwardVector(),
                Up
            ).GetSafeNormal()
            : FVector::ForwardVector;
    }

    const FVector Origin =
        Body.GetLocation()
        + Up * LidarHeightAboveCabPivotCentimeters;
    OutTransform = FTransform(Forward.Rotation(), Origin);
    return true;
}
