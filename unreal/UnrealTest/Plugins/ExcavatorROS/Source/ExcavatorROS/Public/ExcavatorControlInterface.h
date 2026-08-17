#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ExcavatorControlInterface.generated.h"

USTRUCT(BlueprintType)
struct EXCAVATORROS_API FExcavatorNormalizedCommand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    float Throttle = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    float Steering = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    float Swing = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    float Boom = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    float Stick = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    float Bucket = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    bool bEmergencyStop = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    bool bClearEmergencyStop = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Excavator|ROS")
    bool bResetMachine = false;
};

UINTERFACE(BlueprintType)
class EXCAVATORROS_API UExcavatorControlInterface : public UInterface
{
    GENERATED_BODY()
};

class EXCAVATORROS_API IExcavatorControlInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintNativeEvent,
        BlueprintCallable,
        Category = "Excavator|ROS"
    )
    void ApplyExcavatorCommand(const FExcavatorNormalizedCommand& Command);
};
