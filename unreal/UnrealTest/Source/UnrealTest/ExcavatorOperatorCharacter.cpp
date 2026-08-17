#include "ExcavatorOperatorCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

AExcavatorOperatorCharacter::AExcavatorOperatorCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    Movement->JumpZVelocity = 460.0f;
    Movement->AirControl = 0.28f;
    Movement->GravityScale = 0.72f;
    Movement->MaxWalkSpeed = 480.0f;
    Movement->MinAnalogWalkSpeed = 15.0f;
    Movement->BrakingDecelerationWalking = 1800.0f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("OperatorCameraBoom")
    );
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 410.0f;
    CameraBoom->SocketOffset = FVector(0.0f, 52.0f, 62.0f);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 12.0f;
    CameraBoom->bDoCollisionTest = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("OperatorFollowCamera")
    );
    FollowCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName
    );
    FollowCamera->bUsePawnControlRotation = false;
    FollowCamera->FieldOfView = 82.0f;
    FollowCamera->PostProcessBlendWeight = 1.0f;
    FollowCamera->PostProcessSettings.bOverride_DepthOfFieldEnabled = true;
    FollowCamera->PostProcessSettings.DepthOfFieldEnabled = false;
    FollowCamera->PostProcessSettings.bOverride_DepthOfFieldScale = true;
    FollowCamera->PostProcessSettings.DepthOfFieldScale = 0.0f;
    FollowCamera->PostProcessSettings.bOverride_DepthOfFieldNearBlurSize =
        true;
    FollowCamera->PostProcessSettings.DepthOfFieldNearBlurSize = 0.0f;
    FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFarBlurSize =
        true;
    FollowCamera->PostProcessSettings.DepthOfFieldFarBlurSize = 0.0f;
    FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    FollowCamera->PostProcessSettings.MotionBlurAmount = 0.0f;

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> OperatorMesh(
        TEXT(
            "/Game/Characters/Mannequins/Meshes/"
            "SKM_Manny_Simple.SKM_Manny_Simple"
        )
    );
    if (OperatorMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(OperatorMesh.Object);
    }

    static ConstructorHelpers::FClassFinder<UAnimInstance> OperatorAnimation(
        TEXT(
            "/Game/Characters/Mannequins/Anims/Unarmed/"
            "ABP_Unarmed"
        )
    );
    if (OperatorAnimation.Succeeded())
    {
        GetMesh()->SetAnimInstanceClass(OperatorAnimation.Class);
    }

    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AExcavatorOperatorCharacter::Tick(const float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        bJumpButtonWasDown = false;
        return;
    }

    float MoveForward = ApplyDeadzone(
        PlayerController->GetInputAnalogKeyState(
            EKeys::Gamepad_LeftY
        )
    );
    MoveForward += RemoteCommand.Throttle;
    float MoveRight = ApplyDeadzone(
        PlayerController->GetInputAnalogKeyState(
            EKeys::Gamepad_LeftX
        )
    );
    MoveRight += RemoteCommand.Steering;

    MoveForward +=
        PlayerController->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f;
    MoveForward -=
        PlayerController->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f;
    MoveRight +=
        PlayerController->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f;
    MoveRight -=
        PlayerController->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f;
    MoveForward = FMath::Clamp(MoveForward, -1.0f, 1.0f);
    MoveRight = FMath::Clamp(MoveRight, -1.0f, 1.0f);

    const FRotator ControlRotation =
        PlayerController->GetControlRotation();
    const FRotator YawRotation(
        0.0f,
        ControlRotation.Yaw,
        0.0f
    );
    AddMovementInput(
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X),
        MoveForward
    );
    AddMovementInput(
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y),
        MoveRight
    );

    const float LookX = FMath::Clamp(
        ApplyDeadzone(
        PlayerController->GetInputAnalogKeyState(
            EKeys::Gamepad_RightX
        )
        ) + RemoteCommand.Swing,
        -1.0f,
        1.0f
    );
    const float LookY = FMath::Clamp(
        ApplyDeadzone(
        PlayerController->GetInputAnalogKeyState(
            EKeys::Gamepad_RightY
        )
        ) + RemoteCommand.Boom,
        -1.0f,
        1.0f
    );
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    PlayerController->GetInputMouseDelta(MouseX, MouseY);

    FRotator NewControlRotation = ControlRotation;
    NewControlRotation.Yaw +=
        LookX * CameraTurnRateDegreesPerSecond * DeltaTime
        + MouseX * MouseLookSensitivity;
    NewControlRotation.Pitch = FMath::ClampAngle(
        NewControlRotation.Pitch
            - LookY * CameraTurnRateDegreesPerSecond * DeltaTime
            + MouseY * MouseLookSensitivity,
        -70.0f,
        35.0f
    );
    NewControlRotation.Roll = 0.0f;
    PlayerController->SetControlRotation(NewControlRotation);

    const bool bJumpButtonDown =
        PlayerController->IsInputKeyDown(
            EKeys::Gamepad_FaceButton_Bottom
        )
        || PlayerController->IsInputKeyDown(EKeys::SpaceBar)
        || RemoteCommand.Bucket > 0.5f;
    if (bJumpButtonDown && !bJumpButtonWasDown)
    {
        Jump();
    }
    else if (!bJumpButtonDown && bJumpButtonWasDown)
    {
        StopJumping();
    }
    bJumpButtonWasDown = bJumpButtonDown;
}

void AExcavatorOperatorCharacter::ApplyExcavatorCommand_Implementation(
    const FExcavatorNormalizedCommand& Command
)
{
    RemoteCommand = Command;
}

float AExcavatorOperatorCharacter::ApplyDeadzone(
    const float Value
) const
{
    const float Magnitude = FMath::Abs(Value);
    if (Magnitude <= GamepadDeadzone)
    {
        return 0.0f;
    }

    return FMath::Sign(Value)
        * FMath::Clamp(
            (Magnitude - GamepadDeadzone)
                / FMath::Max(1.0f - GamepadDeadzone, KINDA_SMALL_NUMBER),
            0.0f,
            1.0f
        );
}
