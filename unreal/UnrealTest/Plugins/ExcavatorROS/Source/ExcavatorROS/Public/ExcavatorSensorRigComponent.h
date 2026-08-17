#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ExcavatorSensorRigComponent.generated.h"

class UCameraComponent;
class UExcavatorROSBridgeComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;

UCLASS(
    ClassGroup = (ROS),
    BlueprintType,
    Blueprintable,
    meta = (BlueprintSpawnableComponent)
)
class EXCAVATORROS_API UExcavatorSensorRigComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UExcavatorSensorRigComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Lidar",
        meta = (ClampMin = "36", ClampMax = "1440")
    )
    int32 LidarRayCount = 360;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Lidar",
        meta = (ClampMin = "1.0", ClampMax = "30.0")
    )
    float LidarRateHz = 10.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Lidar",
        meta = (ClampMin = "1.0", ClampMax = "100.0")
    )
    float LidarMaximumRangeMeters = 30.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Lidar",
        meta = (ClampMin = "50.0", ClampMax = "400.0")
    )
    float LidarHeightAboveCabPivotCentimeters = 215.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Lidar"
    )
    bool bEnableLidar = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Camera Stream"
    )
    // Pixel Streaming 2 is the primary hardware-H.264 path. This smaller JPEG
    // feed remains active as a network-independent phone fallback.
    bool bEnableCameraStream = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Camera Stream",
        meta = (ClampMin = "1.0", ClampMax = "15.0")
    )
    float CameraStreamRateHz = 8.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Camera Stream",
        meta = (ClampMin = "160", ClampMax = "1920")
    )
    int32 CameraStreamWidth = 640;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Camera Stream",
        meta = (ClampMin = "90", ClampMax = "1080")
    )
    int32 CameraStreamHeight = 360;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Sensors|Camera Stream",
        meta = (ClampMin = "10", ClampMax = "100")
    )
    int32 CameraStreamJpegQuality = 68;

    UFUNCTION(BlueprintCallable, Category = "Excavator|Sensors|Camera")
    void SetActiveCameraView(int32 ViewIndex);

    UFUNCTION(BlueprintCallable, Category = "Excavator|Sensors|Camera")
    void CycleCameraView(int32 Direction = 1);

    UFUNCTION(BlueprintCallable, Category = "Excavator|Sensors|Camera")
    void SetOperatorMode(bool bOnFoot);

    UFUNCTION(BlueprintPure, Category = "Excavator|Sensors|Camera")
    int32 GetActiveCameraView() const
    {
        return ActiveViewIndex;
    }

    bool GetLidarWorldTransform(FTransform& OutTransform) const;

private:
    TWeakObjectPtr<USkeletalMeshComponent> ExcavatorMesh;
    TWeakObjectPtr<UExcavatorROSBridgeComponent> ROSBridge;
    TArray<TWeakObjectPtr<UCameraComponent>> ViewCameras;
    TWeakObjectPtr<USceneCaptureComponent2D> StreamCapture;
    TWeakObjectPtr<UTextureRenderTarget2D> StreamRenderTarget;
    TArray<FString> ViewNames;
    int32 ActiveViewIndex = 0;
    int32 PreviousVehicleViewIndex = 0;
    int32 LastROSRequestedView = INDEX_NONE;
    double NextLidarTimeSeconds = 0.0;
    double NextCameraStreamTimeSeconds = 0.0;
    bool bCycleForwardWasDown = false;
    bool bCycleBackwardWasDown = false;
    bool bOperatorMode = false;

    void ResolveComponents();
    void InitializeCameraViews();
    void InitializeCameraStream();
    void CaptureCameraStreamFrame();
    void UpdateCameraTransforms();
    void UpdateCameraInput();
    void ScanLidar();
    FTransform GetBoneTransform(FName BoneName) const;
};
