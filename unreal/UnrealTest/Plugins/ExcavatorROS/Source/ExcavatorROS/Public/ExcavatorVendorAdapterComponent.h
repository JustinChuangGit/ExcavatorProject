#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ExcavatorControlInterface.h"
#include "ExcavatorVendorAdapterComponent.generated.h"

class APlayerController;
class UAnimInstance;
class USkeletalMeshComponent;
class USpringArmComponent;

UCLASS(
    ClassGroup = (ROS),
    BlueprintType,
    Blueprintable,
    meta = (BlueprintSpawnableComponent)
)
class EXCAVATORROS_API UExcavatorVendorAdapterComponent final
    : public UActorComponent,
      public IExcavatorControlInterface
{
    GENERATED_BODY()

public:
    UExcavatorVendorAdapterComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Adapter",
        meta = (ClampMin = "0.0", ClampMax = "0.5")
    )
    float ArmDeadband = 0.05f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Adapter",
        meta = (ClampMin = "1.0", ClampMax = "3.0")
    )
    float HydraulicResponseExponent = 1.5f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Adapter",
        meta = (ClampMin = "1.0", ClampMax = "90.0")
    )
    float SwingRateDegreesPerSecond = 30.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Adapter",
        meta = (ClampMin = "1.0", ClampMax = "90.0")
    )
    float BoomRateDegreesPerSecond = 20.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Adapter",
        meta = (ClampMin = "1.0", ClampMax = "90.0")
    )
    float StickRateDegreesPerSecond = 25.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Adapter",
        meta = (ClampMin = "1.0", ClampMax = "120.0")
    )
    float BucketRateDegreesPerSecond = 35.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Adapter",
        meta = (ClampMin = "1.0", ClampMax = "20.0")
    )
    float HydraulicSoftStopDegrees = 8.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Input"
    )
    bool bRequireRightBumperForCamera = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Input"
    )
    bool bOverrideInitialCameraRotation = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Input",
        meta = (EditCondition = "bOverrideInitialCameraRotation")
    )
    FRotator InitialCameraRotationOverride =
        FRotator(-18.0f, 0.0f, 0.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|Safety"
    )
    bool bStabilizeChassisDuringHydraulics = true;

    virtual void ApplyExcavatorCommand_Implementation(
        const FExcavatorNormalizedCommand& Command
    ) override;

    uint32 GetResetGeneration() const
    {
        return ResetGeneration;
    }

    /**
     * Parks the machine while the local player is walking around it. ROS can
     * continue publishing normally, but its motion axes are ignored until the
     * operator re-enters the excavator.
     */
    UFUNCTION(BlueprintCallable, Category = "Excavator|Operator")
    void SetOperatorOnFoot(bool bOnFoot);

    UFUNCTION(BlueprintPure, Category = "Excavator|Operator")
    bool IsOperatorOnFoot() const
    {
        return bOperatorOnFoot;
    }

    float GetBucketInput() const
    {
        return LatestCommand.Bucket;
    }

    void SetSoilPenetrationBlocked(const bool bBlocked)
    {
        bSoilPenetrationBlocked = bBlocked;
    }

    bool GetJointStateDegrees(
        float& OutCabSwing,
        float& OutBoom,
        float& OutStick,
        float& OutBucket
    ) const;

private:
    FExcavatorNormalizedCommand LatestCommand;
    FTransform InitialActorTransform;
    TMap<FName, double> InitialVendorFloats;
    TMap<FName, double> ControlledVendorFloats;
    TWeakObjectPtr<USkeletalMeshComponent> VendorMesh;
    TWeakObjectPtr<UAnimInstance> VendorAnimInstance;
    TWeakObjectPtr<USpringArmComponent> CameraSpringArm;
    TWeakObjectPtr<APlayerController> LocalPlayerController;
    FRotator InitialSpringArmRotation;
    FRotator LockedSpringArmRotation;
    FRotator InitialControlRotation;
    FRotator LockedControlRotation;
    FTransform ChassisAnchorTransform;
    bool bResetWasActive = false;
    bool bSpringArmRotationCaptured = false;
    bool bControlRotationCaptured = false;
    bool bUpperBodyCollisionDisabled = false;
    bool bChassisAnchored = false;
    bool bSoilPenetrationBlocked = false;
    bool bOperatorOnFoot = false;
    uint32 ResetGeneration = 0;

    void ApplyLatestCommandToVendor();
    void ApplyProportionalHydraulicMotion(float DeltaTime);
    void IntegrateVendorPose(
        FName PropertyName,
        float NormalizedAxis,
        float RateDegreesPerSecond,
        float MinimumDegrees,
        float MaximumDegrees,
        float DeltaTime,
        bool bWrapAngle = false
    );
    void CaptureInitialVendorState();
    void ResolveVendorAnimation();
    void DisableUpperBodyPhysicsCollision();
    void UpdateChassisStabilization();
    void ResetMachineToInitialState();
    void UpdateCameraInputGate();
    void ResolveCameraComponents();
    bool SetVendorBool(const FName PropertyName, bool bValue) const;
    bool GetVendorNumber(const FName PropertyName, double& OutValue) const;
    bool SetVendorNumber(const FName PropertyName, double Value) const;
};
