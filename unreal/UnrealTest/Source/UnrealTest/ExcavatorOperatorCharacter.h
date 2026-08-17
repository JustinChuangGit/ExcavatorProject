#pragma once

#include "CoreMinimal.h"
#include "ExcavatorControlInterface.h"
#include "GameFramework/Character.h"

#include "ExcavatorOperatorCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Lightweight third-person operator used when the player exits the excavator.
 *
 * Input is read directly so the character works alongside the Marketplace
 * excavator's existing input setup without requiring another input mapping
 * context. The terrain actor owns the Y-button enter/exit toggle.
 */
UCLASS()
class UNREALTEST_API AExcavatorOperatorCharacter final
    : public ACharacter,
      public IExcavatorControlInterface
{
    GENERATED_BODY()

public:
    AExcavatorOperatorCharacter();

    virtual void Tick(float DeltaTime) override;

    virtual void ApplyExcavatorCommand_Implementation(
        const FExcavatorNormalizedCommand& Command
    ) override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Operator|Camera"
    )
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Operator|Camera"
    )
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Operator|Input",
        meta = (ClampMin = "0.0", ClampMax = "0.5")
    )
    float GamepadDeadzone = 0.12f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Operator|Input",
        meta = (ClampMin = "30.0", ClampMax = "360.0")
    )
    float CameraTurnRateDegreesPerSecond = 115.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Operator|Input",
        meta = (ClampMin = "0.01", ClampMax = "1.0")
    )
    float MouseLookSensitivity = 0.12f;

private:
    FExcavatorNormalizedCommand RemoteCommand;
    bool bJumpButtonWasDown = false;

    float ApplyDeadzone(float Value) const;
};
