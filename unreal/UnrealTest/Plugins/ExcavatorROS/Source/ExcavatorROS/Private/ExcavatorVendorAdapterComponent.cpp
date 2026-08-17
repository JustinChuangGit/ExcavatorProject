#include "ExcavatorVendorAdapterComponent.h"

#include "Animation/AnimInstance.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ExcavatorSensorRigComponent.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogExcavatorVendorAdapter, Log, All);

namespace ExcavatorVendorProperties
{
const FName SwingLeft = TEXT("BodyRotate");
const FName SwingRight = TEXT("BodyRotat02");
const FName BoomUp = TEXT("WantArm02TurnUp");
const FName BoomDown = TEXT("WantArm02TurnDown");
const FName StickUp = TEXT("WantArm04Up");
const FName StickDown = TEXT("WantArm04Down");
const FName BucketCurl = TEXT("WantEndRotate");
const FName BucketDump = TEXT("WantEndRotateDown");
const FName Arm03Up = TEXT("WantArm03Rotateup");
const FName Arm03Down = TEXT("WantArm03Rotatedown");
const FName Arm01Left = TEXT("WantTurnArmLeft01");
const FName Arm01Right = TEXT("WantTurnArmright01");
const FName CabPose = TEXT("BodyRotate");
const FName Arm01Pose = TEXT("Arm01Rotate");
const FName BoomPose = TEXT("Arm02Rotate");
const FName Arm03Pose = TEXT("Arm03Rotate");
const FName StickPose = TEXT("Arm04Rotate");
const FName BucketPose = TEXT("EndRotate");

const FName PoseNumbers[] = {
    CabPose,
    Arm01Pose,
    BoomPose,
    Arm03Pose,
    StickPose,
    BucketPose
};

const FName MovingUpperBodyBones[] = {
    TEXT("B_ConstractionExcavator01_Body"),
    TEXT("B_ConstractionExcavator01_Arm01"),
    TEXT("B_ConstractionExcavator01_Arm02"),
    TEXT("B_ConstractionExcavator01_Arm03"),
    TEXT("B_ConstractionExcavator01_Arm04"),
    TEXT("B_ConstractionExcavator01_End"),
    TEXT("B_ConstractionExcavator01_End01"),
    TEXT("B_ConstractionExcavator01_End03"),
    TEXT("B_ConstractionExcavator01_Cable01D"),
    TEXT("B_ConstractionExcavator01_Cable01U"),
    TEXT("B_ConstractionExcavator01_Cable02D"),
    TEXT("B_ConstractionExcavator01_Cable02U"),
    TEXT("B_ConstractionExcavator01_Cable03D"),
    TEXT("B_ConstractionExcavator01_Cable03U"),
    TEXT("B_ConstractionExcavator01_Cable04D"),
    TEXT("B_ConstractionExcavator01_Cable04U"),
    TEXT("B_ConstractionExcavator01_Hyd01D"),
    TEXT("B_ConstractionExcavator01_Hyd01U"),
    TEXT("B_ConstractionExcavator01_Hyd02D"),
    TEXT("B_ConstractionExcavator01_Hyd02U"),
    TEXT("B_ConstractionExcavator01_Hyd03D"),
    TEXT("B_ConstractionExcavator01_Hyd03U"),
    TEXT("B_ConstractionExcavator01_Hyd04D"),
    TEXT("B_ConstractionExcavator01_Hyd04U"),
    TEXT("B_ConstractionExcavator01_Hyd05D"),
    TEXT("B_ConstractionExcavator01_Hyd05U")
};
}

UExcavatorVendorAdapterComponent::UExcavatorVendorAdapterComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UExcavatorVendorAdapterComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AActor* Owner = GetOwner())
    {
        InitialActorTransform = Owner->GetActorTransform();
        AddTickPrerequisiteActor(Owner);
        if (UChaosWheeledVehicleMovementComponent* Movement =
                Owner->FindComponentByClass<
                    UChaosWheeledVehicleMovementComponent
                >())
        {
            // The Marketplace Blueprint stores its own copy of these protected
            // Chaos settings and can override the engine defaults. Enforce the
            // arcade-style direction switching expected by our RT/LT controls.
            if (FBoolProperty* ReverseAsBrake = FindFProperty<FBoolProperty>(
                    Movement->GetClass(),
                    TEXT("bReverseAsBrake")
                ))
            {
                ReverseAsBrake->SetPropertyValue_InContainer(Movement, true);
            }
            if (FBoolProperty* ThrottleAsBrake = FindFProperty<FBoolProperty>(
                    Movement->GetClass(),
                    TEXT("bThrottleAsBrake")
                ))
            {
                ThrottleAsBrake->SetPropertyValue_InContainer(Movement, true);
            }
        }
        if (!Owner->FindComponentByClass<UExcavatorSensorRigComponent>())
        {
            UExcavatorSensorRigComponent* SensorRig =
                NewObject<UExcavatorSensorRigComponent>(
                    Owner,
                    TEXT("ExcavatorSensorRig")
                );
            Owner->AddInstanceComponent(SensorRig);
            SensorRig->RegisterComponent();
        }
    }

    ResolveVendorAnimation();
    CaptureInitialVendorState();
    DisableUpperBodyPhysicsCollision();
    ResolveCameraComponents();
    if (bOverrideInitialCameraRotation && !bOperatorOnFoot)
    {
        if (CameraSpringArm.IsValid())
        {
            CameraSpringArm->SetRelativeRotation(
                InitialCameraRotationOverride
            );
            InitialSpringArmRotation = InitialCameraRotationOverride;
            LockedSpringArmRotation = InitialCameraRotationOverride;
            bSpringArmRotationCaptured = true;
        }
        if (LocalPlayerController.IsValid())
        {
            LocalPlayerController->SetControlRotation(
                InitialCameraRotationOverride
            );
            InitialControlRotation = InitialCameraRotationOverride;
            LockedControlRotation = InitialCameraRotationOverride;
            bControlRotationCaptured = true;
        }
        UE_LOG(
            LogExcavatorVendorAdapter,
            Log,
            TEXT("Applied initial camera rotation override %s"),
            *InitialCameraRotationOverride.ToCompactString()
        );
    }
}

void UExcavatorVendorAdapterComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ResolveVendorAnimation();
    if (InitialVendorFloats.IsEmpty())
    {
        CaptureInitialVendorState();
    }
    DisableUpperBodyPhysicsCollision();

    // Integrate the normalized ROS axes directly into persistent animation
    // targets. Deflection is joint velocity; a centered stick adds zero and
    // therefore holds the current pose.
    ApplyProportionalHydraulicMotion(DeltaTime);
    UpdateChassisStabilization();

    // The vendor Blueprint also consumes Enhanced Input. Reapplying the ROS
    // command after its actor tick makes ROS the single authority and prevents
    // stick input from leaking into wheel steering.
    ApplyLatestCommandToVendor();
    UpdateCameraInputGate();
}

void UExcavatorVendorAdapterComponent::ApplyExcavatorCommand_Implementation(
    const FExcavatorNormalizedCommand& Command
)
{
    if (bOperatorOnFoot)
    {
        // Keep consuming the ROS stream so the bridge remains healthy, but do
        // not let a held stick move an unoccupied machine. Reset remains
        // available from Mission Control even while the operator is outside.
        const bool bResetOnThisCommand =
            Command.bResetMachine && !bResetWasActive;
        bResetWasActive = Command.bResetMachine;
        LatestCommand = FExcavatorNormalizedCommand();
        FExcavatorNormalizedCommand OperatorCommand = Command;
        if (Command.bResetMachine)
        {
            OperatorCommand.Throttle = 0.0f;
            OperatorCommand.Steering = 0.0f;
            OperatorCommand.Swing = 0.0f;
            OperatorCommand.Boom = 0.0f;
            OperatorCommand.Stick = 0.0f;
            OperatorCommand.Bucket = 0.0f;
        }
        UWorld* World = GetWorld();
        APlayerController* PlayerController =
            World ? World->GetFirstPlayerController() : nullptr;
        APawn* OperatorPawn =
            PlayerController ? PlayerController->GetPawn() : nullptr;
        if (
            OperatorPawn
            && OperatorPawn != GetOwner()
            && OperatorPawn->GetClass()->ImplementsInterface(
                UExcavatorControlInterface::StaticClass()
            )
        )
        {
            IExcavatorControlInterface::Execute_ApplyExcavatorCommand(
                OperatorPawn,
                OperatorCommand
            );
        }
        if (bResetOnThisCommand)
        {
            ResetMachineToInitialState();
        }
        ApplyLatestCommandToVendor();
        return;
    }

    const bool bResetOnThisCommand =
        Command.bResetMachine && !bResetWasActive;
    bResetWasActive = Command.bResetMachine;
    LatestCommand = Command;

    if (Command.bResetMachine)
    {
        LatestCommand.Throttle = 0.0f;
        LatestCommand.Steering = 0.0f;
        LatestCommand.Swing = 0.0f;
        LatestCommand.Boom = 0.0f;
        LatestCommand.Stick = 0.0f;
        LatestCommand.Bucket = 0.0f;
    }

    if (bResetOnThisCommand)
    {
        ResetMachineToInitialState();
    }

    ApplyLatestCommandToVendor();
}

void UExcavatorVendorAdapterComponent::SetOperatorOnFoot(
    const bool bOnFoot
)
{
    if (bOperatorOnFoot == bOnFoot)
    {
        return;
    }

    bOperatorOnFoot = bOnFoot;
    if (bOperatorOnFoot)
    {
        LatestCommand = FExcavatorNormalizedCommand();
        bResetWasActive = false;
        ApplyLatestCommandToVendor();
    }
    else
    {
        // The player controller has just changed pawns. Resolve it again and
        // make the excavator camera's current pose the new locked pose.
        LocalPlayerController.Reset();
        ResolveCameraComponents();
        if (LocalPlayerController.IsValid())
        {
            LockedControlRotation =
                LocalPlayerController->GetControlRotation();
            bControlRotationCaptured = true;
        }
        if (CameraSpringArm.IsValid())
        {
            LockedSpringArmRotation =
                CameraSpringArm->GetRelativeRotation();
            bSpringArmRotationCaptured = true;
        }
    }

    UE_LOG(
        LogExcavatorVendorAdapter,
        Log,
        TEXT("Operator mode: %s"),
        bOperatorOnFoot ? TEXT("on foot") : TEXT("in excavator")
    );
}

bool UExcavatorVendorAdapterComponent::GetJointStateDegrees(
    float& OutCabSwing,
    float& OutBoom,
    float& OutStick,
    float& OutBucket
) const
{
    double Cab = 0.0;
    double Boom = 0.0;
    double Stick = 0.0;
    double Bucket = 0.0;
    const bool bValid =
        GetVendorNumber(ExcavatorVendorProperties::CabPose, Cab)
        && GetVendorNumber(ExcavatorVendorProperties::BoomPose, Boom)
        && GetVendorNumber(ExcavatorVendorProperties::StickPose, Stick)
        && GetVendorNumber(ExcavatorVendorProperties::BucketPose, Bucket);
    OutCabSwing = static_cast<float>(Cab);
    OutBoom = static_cast<float>(Boom);
    OutStick = static_cast<float>(Stick);
    OutBucket = static_cast<float>(Bucket);
    return bValid;
}

void UExcavatorVendorAdapterComponent::ApplyLatestCommandToVendor()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    UChaosWheeledVehicleMovementComponent* Movement =
        Owner->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
    if (Movement)
    {
        const bool bStopped = LatestCommand.bEmergencyStop;
        const bool bHoldChassis = bStopped || bChassisAnchored;
        const float DriveInput = bHoldChassis
            ? 0.0f
            : FMath::Clamp(LatestCommand.Throttle, -1.0f, 1.0f);
        const float ForwardThrottle = FMath::Max(DriveInput, 0.0f);
        const float ReverseThrottle = FMath::Max(-DriveInput, 0.0f);

        // Chaos vehicles use "reverse as brake" by default: a positive brake
        // input first stops the chassis, selects reverse, and then becomes
        // proportional reverse throttle. Passing a negative throttle never
        // reaches that gear-change path.
        Movement->SetThrottleInput(ForwardThrottle);
        Movement->SetSteeringInput(
            bHoldChassis ? 0.0f : LatestCommand.Steering
        );
        Movement->SetBrakeInput(bHoldChassis ? 1.0f : ReverseThrottle);
        Movement->SetHandbrakeInput(bHoldChassis);
    }

    // Do not hand hydraulic commands back to the Marketplace Blueprint. Those
    // flags are fixed-speed and interpolate back to zero when released.
    SetVendorBool(ExcavatorVendorProperties::SwingLeft, false);
    SetVendorBool(ExcavatorVendorProperties::SwingRight, false);
    SetVendorBool(ExcavatorVendorProperties::BoomUp, false);
    SetVendorBool(ExcavatorVendorProperties::BoomDown, false);
    SetVendorBool(ExcavatorVendorProperties::StickUp, false);
    SetVendorBool(ExcavatorVendorProperties::StickDown, false);
    SetVendorBool(ExcavatorVendorProperties::BucketCurl, false);
    SetVendorBool(ExcavatorVendorProperties::BucketDump, false);
    SetVendorBool(ExcavatorVendorProperties::Arm03Up, false);
    SetVendorBool(ExcavatorVendorProperties::Arm03Down, false);
    SetVendorBool(ExcavatorVendorProperties::Arm01Left, false);
    SetVendorBool(ExcavatorVendorProperties::Arm01Right, false);
}

void UExcavatorVendorAdapterComponent::ApplyProportionalHydraulicMotion(
    const float DeltaTime
)
{
    const bool bStopped = LatestCommand.bEmergencyStop;
    const float BoomInput =
        bSoilPenetrationBlocked && LatestCommand.Boom < 0.0f
        ? 0.0f
        : LatestCommand.Boom;
    const float StickInput =
        bSoilPenetrationBlocked && LatestCommand.Stick < 0.0f
        ? 0.0f
        : LatestCommand.Stick;
    IntegrateVendorPose(
        ExcavatorVendorProperties::CabPose,
        bStopped ? 0.0f : LatestCommand.Swing,
        SwingRateDegreesPerSecond,
        -180.0f,
        180.0f,
        DeltaTime,
        true
    );
    IntegrateVendorPose(
        ExcavatorVendorProperties::BoomPose,
        bStopped ? 0.0f : BoomInput,
        BoomRateDegreesPerSecond,
        -35.0f,
        35.0f,
        DeltaTime
    );
    IntegrateVendorPose(
        ExcavatorVendorProperties::StickPose,
        bStopped ? 0.0f : StickInput,
        StickRateDegreesPerSecond,
        -55.0f,
        55.0f,
        DeltaTime
    );
    IntegrateVendorPose(
        ExcavatorVendorProperties::BucketPose,
        bStopped ? 0.0f : LatestCommand.Bucket,
        BucketRateDegreesPerSecond,
        -75.0f,
        75.0f,
        DeltaTime
    );

    // These vendor joints are not part of the four-axis ROS layout. Rewriting
    // their saved targets each frame blocks the asset's direct controller
    // input from leaking into the implement.
    IntegrateVendorPose(
        ExcavatorVendorProperties::Arm01Pose,
        0.0f,
        1.0f,
        -75.0f,
        75.0f,
        DeltaTime
    );
    IntegrateVendorPose(
        ExcavatorVendorProperties::Arm03Pose,
        0.0f,
        1.0f,
        -75.0f,
        75.0f,
        DeltaTime
    );
}

void UExcavatorVendorAdapterComponent::IntegrateVendorPose(
    const FName PropertyName,
    const float NormalizedAxis,
    const float RateDegreesPerSecond,
    const float MinimumDegrees,
    const float MaximumDegrees,
    const float DeltaTime,
    const bool bWrapAngle
)
{
    double CurrentValue = 0.0;
    if (!GetVendorNumber(PropertyName, CurrentValue))
    {
        return;
    }

    double* ControlledValue = ControlledVendorFloats.Find(PropertyName);
    if (!ControlledValue)
    {
        ControlledVendorFloats.Add(PropertyName, CurrentValue);
        ControlledValue = ControlledVendorFloats.Find(PropertyName);
    }

    const float ClampedAxis =
        FMath::Clamp(NormalizedAxis, -1.0f, 1.0f);
    const float AxisMagnitude = FMath::Abs(ClampedAxis);
    const float SignedResponse = AxisMagnitude <= ArmDeadband
        ? 0.0f
        : FMath::Sign(ClampedAxis)
            * FMath::Pow(AxisMagnitude, HydraulicResponseExponent);

    const float SoftStopRange =
        FMath::Max(HydraulicSoftStopDegrees, 0.01f);
    float SoftStopScale = 1.0f;
    if (!bWrapAngle && SignedResponse > 0.0f)
    {
        const double RemainingDegrees =
            static_cast<double>(MaximumDegrees) - *ControlledValue;
        SoftStopScale = FMath::Clamp(
            static_cast<float>(
                RemainingDegrees / SoftStopRange
            ),
            0.0f,
            1.0f
        );
    }
    else if (!bWrapAngle && SignedResponse < 0.0f)
    {
        const double RemainingDegrees =
            *ControlledValue - static_cast<double>(MinimumDegrees);
        SoftStopScale = FMath::Clamp(
            static_cast<float>(
                RemainingDegrees / SoftStopRange
            ),
            0.0f,
            1.0f
        );
    }

    double NextValue = *ControlledValue
        + static_cast<double>(
            SignedResponse
            * SoftStopScale
            * RateDegreesPerSecond
            * DeltaTime
        );
    if (bWrapAngle)
    {
        NextValue = FMath::UnwindDegrees(static_cast<float>(NextValue));
    }
    else
    {
        NextValue = FMath::Clamp(
            NextValue,
            static_cast<double>(MinimumDegrees),
            static_cast<double>(MaximumDegrees)
        );
    }

    *ControlledValue = NextValue;
    SetVendorNumber(PropertyName, NextValue);
}

void UExcavatorVendorAdapterComponent::CaptureInitialVendorState()
{
    InitialVendorFloats.Empty();
    ControlledVendorFloats.Empty();
    for (const FName PropertyName : ExcavatorVendorProperties::PoseNumbers)
    {
        double Value = 0.0;
        if (GetVendorNumber(PropertyName, Value))
        {
            InitialVendorFloats.Add(PropertyName, Value);
            ControlledVendorFloats.Add(PropertyName, Value);
        }
    }

    UE_LOG(
        LogExcavatorVendorAdapter,
        Log,
        TEXT("Captured %d direct animation pose targets"),
        InitialVendorFloats.Num()
    );
}

void UExcavatorVendorAdapterComponent::ResolveVendorAnimation()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (!VendorMesh.IsValid())
    {
        VendorMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
        if (VendorMesh.IsValid())
        {
            // The vehicle Blueprint writes its targets during the actor tick.
            // Run this adapter next, then let the animation consume the
            // corrected analog values.
            VendorMesh->PrimaryComponentTick.AddPrerequisite(
                this,
                PrimaryComponentTick
            );
        }
    }

    if (VendorMesh.IsValid())
    {
        UAnimInstance* CurrentAnimInstance = VendorMesh->GetAnimInstance();
        if (CurrentAnimInstance != VendorAnimInstance.Get())
        {
            VendorAnimInstance = CurrentAnimInstance;
            InitialVendorFloats.Empty();
            ControlledVendorFloats.Empty();
        }
    }
}

void UExcavatorVendorAdapterComponent::DisableUpperBodyPhysicsCollision()
{
    if (bUpperBodyCollisionDisabled || !VendorMesh.IsValid())
    {
        return;
    }

    int32 DisabledBodyCount = 0;
    for (
        const FName BoneName
        : ExcavatorVendorProperties::MovingUpperBodyBones
    )
    {
        if (FBodyInstance* BodyInstance =
            VendorMesh->GetBodyInstance(BoneName))
        {
            BodyInstance->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ++DisabledBodyCount;
        }
    }

    bUpperBodyCollisionDisabled = true;
    UE_LOG(
        LogExcavatorVendorAdapter,
        Log,
        TEXT("Disabled physics collision on %d moving upper-body bones"),
        DisabledBodyCount
    );
}

void UExcavatorVendorAdapterComponent::UpdateChassisStabilization()
{
    if (!bStabilizeChassisDuringHydraulics)
    {
        bChassisAnchored = false;
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    const bool bHydraulicsActive =
        FMath::Abs(LatestCommand.Swing) > ArmDeadband
        || FMath::Abs(LatestCommand.Boom) > ArmDeadband
        || FMath::Abs(LatestCommand.Stick) > ArmDeadband
        || FMath::Abs(LatestCommand.Bucket) > ArmDeadband;
    const bool bDriveRequested =
        FMath::Abs(LatestCommand.Throttle) > ArmDeadband
        || FMath::Abs(LatestCommand.Steering) > ArmDeadband;

    if (bHydraulicsActive && !bChassisAnchored)
    {
        ChassisAnchorTransform = Owner->GetActorTransform();
        bChassisAnchored = true;
        UE_LOG(
            LogExcavatorVendorAdapter,
            Verbose,
            TEXT("Hydraulic chassis stabilizer engaged")
        );
    }
    else if (bChassisAnchored && bDriveRequested && !bHydraulicsActive)
    {
        bChassisAnchored = false;
        UE_LOG(
            LogExcavatorVendorAdapter,
            Verbose,
            TEXT("Hydraulic chassis stabilizer released for driving")
        );
    }

    if (!bChassisAnchored)
    {
        return;
    }

    Owner->SetActorTransform(
        ChassisAnchorTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    Owner->GetComponents(PrimitiveComponents);
    for (UPrimitiveComponent* Primitive : PrimitiveComponents)
    {
        if (Primitive && Primitive->IsSimulatingPhysics())
        {
            Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
    }
}

void UExcavatorVendorAdapterComponent::ResetMachineToInitialState()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    ++ResetGeneration;
    bChassisAnchored = false;
    bSoilPenetrationBlocked = false;

    if (
        UChaosWheeledVehicleMovementComponent* Movement =
            Owner->FindComponentByClass<
                UChaosWheeledVehicleMovementComponent
            >()
    )
    {
        Movement->SetThrottleInput(0.0f);
        Movement->SetSteeringInput(0.0f);
        Movement->SetBrakeInput(1.0f);
        Movement->SetHandbrakeInput(true);
        Movement->StopMovementImmediately();
    }

    Owner->SetActorTransform(
        InitialActorTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    Owner->GetComponents(PrimitiveComponents);
    for (UPrimitiveComponent* Primitive : PrimitiveComponents)
    {
        if (Primitive && Primitive->IsSimulatingPhysics())
        {
            Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
    }

    for (const TPair<FName, double>& Pair : InitialVendorFloats)
    {
        SetVendorNumber(Pair.Key, Pair.Value);
        ControlledVendorFloats.FindOrAdd(Pair.Key) = Pair.Value;
    }

    if (CameraSpringArm.IsValid() && bSpringArmRotationCaptured)
    {
        CameraSpringArm->SetRelativeRotation(InitialSpringArmRotation);
        LockedSpringArmRotation = InitialSpringArmRotation;
    }
    if (LocalPlayerController.IsValid() && bControlRotationCaptured)
    {
        LocalPlayerController->SetControlRotation(InitialControlRotation);
        LockedControlRotation = InitialControlRotation;
    }

    UE_LOG(
        LogExcavatorVendorAdapter,
        Log,
        TEXT("Machine reset to its initial transform")
    );
}

void UExcavatorVendorAdapterComponent::ResolveCameraComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (!CameraSpringArm.IsValid())
    {
        CameraSpringArm =
            Owner->FindComponentByClass<USpringArmComponent>();
        if (CameraSpringArm.IsValid())
        {
            InitialSpringArmRotation =
                CameraSpringArm->GetRelativeRotation();
            LockedSpringArmRotation = InitialSpringArmRotation;
            bSpringArmRotationCaptured = true;
        }
    }

    if (!LocalPlayerController.IsValid())
    {
        if (const APawn* Pawn = Cast<APawn>(Owner))
        {
            LocalPlayerController =
                Cast<APlayerController>(Pawn->GetController());
        }
        if (!LocalPlayerController.IsValid() && GetWorld())
        {
            LocalPlayerController = GetWorld()->GetFirstPlayerController();
        }

        if (LocalPlayerController.IsValid())
        {
            InitialControlRotation =
                LocalPlayerController->GetControlRotation();
            LockedControlRotation = InitialControlRotation;
            bControlRotationCaptured = true;
        }
    }
}

void UExcavatorVendorAdapterComponent::UpdateCameraInputGate()
{
    if (bOperatorOnFoot || !bRequireRightBumperForCamera)
    {
        return;
    }

    ResolveCameraComponents();
    if (!LocalPlayerController.IsValid())
    {
        return;
    }

    const bool bCameraMode = LocalPlayerController->IsInputKeyDown(
        EKeys::Gamepad_RightShoulder
    );

    if (bCameraMode)
    {
        if (CameraSpringArm.IsValid())
        {
            LockedSpringArmRotation =
                CameraSpringArm->GetRelativeRotation();
        }
        LockedControlRotation =
            LocalPlayerController->GetControlRotation();
        return;
    }

    if (CameraSpringArm.IsValid() && bSpringArmRotationCaptured)
    {
        CameraSpringArm->SetRelativeRotation(LockedSpringArmRotation);
    }
    if (bControlRotationCaptured)
    {
        LocalPlayerController->SetControlRotation(LockedControlRotation);
    }
}

bool UExcavatorVendorAdapterComponent::SetVendorBool(
    const FName PropertyName,
    const bool bValue
) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    FBoolProperty* Property = FindFProperty<FBoolProperty>(
        Owner->GetClass(),
        PropertyName
    );
    if (!Property)
    {
        UE_LOG(
            LogExcavatorVendorAdapter,
            VeryVerbose,
            TEXT("Vendor property not found: %s"),
            *PropertyName.ToString()
        );
        return false;
    }

    Property->SetPropertyValue_InContainer(Owner, bValue);
    return true;
}

bool UExcavatorVendorAdapterComponent::GetVendorNumber(
    const FName PropertyName,
    double& OutValue
) const
{
    UAnimInstance* AnimInstance = VendorAnimInstance.Get();
    if (!AnimInstance)
    {
        return false;
    }

    FNumericProperty* Property = CastField<FNumericProperty>(
        AnimInstance->GetClass()->FindPropertyByName(PropertyName)
    );
    if (!Property)
    {
        return false;
    }

    const void* ValueAddress =
        Property->ContainerPtrToValuePtr<void>(AnimInstance);
    if (Property->IsFloatingPoint())
    {
        OutValue = Property->GetFloatingPointPropertyValue(ValueAddress);
        return true;
    }
    if (Property->IsInteger())
    {
        OutValue = static_cast<double>(
            Property->GetSignedIntPropertyValue(ValueAddress)
        );
        return true;
    }
    return false;
}

bool UExcavatorVendorAdapterComponent::SetVendorNumber(
    const FName PropertyName,
    const double Value
) const
{
    UAnimInstance* AnimInstance = VendorAnimInstance.Get();
    if (!AnimInstance)
    {
        return false;
    }

    FNumericProperty* Property = CastField<FNumericProperty>(
        AnimInstance->GetClass()->FindPropertyByName(PropertyName)
    );
    if (!Property)
    {
        return false;
    }

    void* ValueAddress =
        Property->ContainerPtrToValuePtr<void>(AnimInstance);
    if (Property->IsFloatingPoint())
    {
        Property->SetFloatingPointPropertyValue(ValueAddress, Value);
        return true;
    }
    if (Property->IsInteger())
    {
        Property->SetIntPropertyValue(
            ValueAddress,
            static_cast<int64>(Value)
        );
        return true;
    }
    return false;
}
