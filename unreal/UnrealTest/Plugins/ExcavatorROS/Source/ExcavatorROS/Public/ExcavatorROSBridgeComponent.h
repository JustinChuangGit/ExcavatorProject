#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ExcavatorControlInterface.h"
#include "ExcavatorROSBridgeComponent.generated.h"

class FJsonObject;
class IWebSocket;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FExcavatorCommandReceived,
    FExcavatorNormalizedCommand,
    Command
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FExcavatorROSConnectionChanged,
    bool,
    bConnected
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FExcavatorCommandTimeoutChanged,
    bool,
    bTimedOut
);

UCLASS(
    ClassGroup = (ROS),
    BlueprintType,
    Blueprintable,
    meta = (BlueprintSpawnableComponent)
)
class EXCAVATORROS_API UExcavatorROSBridgeComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UExcavatorROSBridgeComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Connection"
    )
    FString ServerUrl = TEXT("ws://127.0.0.1:9090/");

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Connection"
    )
    bool bConnectOnBeginPlay = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Connection",
        meta = (ClampMin = "0.25")
    )
    float ReconnectDelaySeconds = 2.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|Safety",
        meta = (ClampMin = "0.05")
    )
    float CommandWatchdogSeconds = 0.5f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Excavator|ROS|State",
        meta = (ClampMin = "1.0", ClampMax = "60.0")
    )
    float StatePublishRateHz = 20.0f;

    UPROPERTY(
        BlueprintAssignable,
        Category = "Excavator|ROS|Events"
    )
    FExcavatorCommandReceived OnCommandReceived;

    UPROPERTY(
        BlueprintAssignable,
        Category = "Excavator|ROS|Events"
    )
    FExcavatorROSConnectionChanged OnConnectionChanged;

    UPROPERTY(
        BlueprintAssignable,
        Category = "Excavator|ROS|Events"
    )
    FExcavatorCommandTimeoutChanged OnCommandTimeoutChanged;

    UFUNCTION(BlueprintCallable, Category = "Excavator|ROS")
    void Connect();

    UFUNCTION(BlueprintCallable, Category = "Excavator|ROS")
    void Disconnect();

    UFUNCTION(BlueprintPure, Category = "Excavator|ROS")
    bool IsConnected() const;

    UFUNCTION(BlueprintPure, Category = "Excavator|ROS")
    bool IsEmergencyStopLatched() const
    {
        return bEmergencyStopLatched;
    }

    UFUNCTION(BlueprintCallable, Category = "Excavator|ROS")
    void PublishState(const FExcavatorNormalizedCommand& CurrentCommand);

    void PublishLaserScan(
        const TArray<float>& RangesMeters,
        float AngleMinimumRadians,
        float AngleMaximumRadians,
        float RangeMinimumMeters,
        float RangeMaximumMeters,
        float ScanTimeSeconds,
        const FString& FrameId
    );

    void PublishActiveCamera(
        int32 ViewIndex,
        const FString& ViewName
    );

    void PublishCompressedCameraImage(
        const uint8* CompressedData,
        uint32 DataSize,
        const FString& Format,
        const FString& FrameId
    );

    UFUNCTION(BlueprintCallable, Category = "Excavator|Operator")
    void SetOperatorOnFoot(bool bOnFoot);

    UFUNCTION(BlueprintPure, Category = "Excavator|Operator")
    bool IsOperatorOnFoot() const
    {
        return bOperatorOnFoot;
    }

    int32 GetRequestedCameraView() const
    {
        return RequestedCameraView;
    }

    uint32 GetOperatorToggleGeneration() const
    {
        return OperatorToggleGeneration;
    }

private:
    TSharedPtr<IWebSocket> Socket;
    double LastCommandTimeSeconds = -1.0;
    double NextReconnectTimeSeconds = 0.0;
    double NextStatePublishTimeSeconds = 0.0;
    FExcavatorNormalizedCommand LastAppliedCommand;
    bool bCommandTimedOut = true;
    bool bEmergencyStopLatched = false;
    bool bIntentionalDisconnect = false;
    bool bOperatorOnFoot = false;
    int32 RequestedCameraView = 0;
    uint32 OperatorToggleGeneration = 0;

    void HandleConnected();
    void HandleConnectionError(const FString& Error);
    void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void HandleMessage(const FString& Message);
    void SendSubscriptions();
    void AdvertiseTopic(const FString& Topic, const FString& Type);
    void SendJson(const TSharedRef<FJsonObject>& Message);
    void DispatchCommand(FExcavatorNormalizedCommand Command);
    void DispatchOperatorToggle();
    void PublishOperatorState();
    void SetCommandTimedOut(bool bNewTimedOut);

    static float ReadNormalizedNumber(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName
    );
};
