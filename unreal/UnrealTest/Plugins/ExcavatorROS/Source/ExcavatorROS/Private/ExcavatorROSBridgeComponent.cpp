#include "ExcavatorROSBridgeComponent.h"

#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "ExcavatorSensorRigComponent.h"
#include "ExcavatorVendorAdapterComponent.h"
#include "GameFramework/Actor.h"
#include "IWebSocket.h"
#include "Misc/Base64.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WebSocketsModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogExcavatorROSBridge, Log, All);

namespace ExcavatorROSTopics
{
const FString Command = TEXT("/excavator/command");
const FString Velocity = TEXT("/excavator/cmd_vel");
const FString EmergencyStop = TEXT("/excavator/emergency_stop");
const FString State = TEXT("/excavator/state");
const FString JointStates = TEXT("/joint_states");
const FString Transforms = TEXT("/tf");
const FString LidarScan = TEXT("/excavator/lidar/scan");
const FString CameraSelect = TEXT("/excavator/camera/select");
const FString CameraActive = TEXT("/excavator/camera/active");
const FString CameraImage =
    TEXT("/excavator/camera/image/compressed");
const FString OperatorToggle = TEXT("/excavator/operator/toggle");
const FString OperatorState = TEXT("/excavator/operator/state");
}

namespace
{
TSharedRef<FJsonObject> MakeROSHeader(const FString& FrameId)
{
    const FDateTime Now = FDateTime::UtcNow();
    const int64 Seconds = Now.ToUnixTimestamp();
    const int64 Nanoseconds =
        (Now.GetTicks() % ETimespan::TicksPerSecond) * 100;
    TSharedRef<FJsonObject> Stamp = MakeShared<FJsonObject>();
    Stamp->SetNumberField(TEXT("sec"), static_cast<double>(Seconds));
    Stamp->SetNumberField(
        TEXT("nanosec"),
        static_cast<double>(Nanoseconds)
    );
    TSharedRef<FJsonObject> Header = MakeShared<FJsonObject>();
    Header->SetObjectField(TEXT("stamp"), Stamp);
    Header->SetStringField(TEXT("frame_id"), FrameId);
    return Header;
}

TSharedRef<FJsonObject> MakeVector(
    const double X,
    const double Y,
    const double Z
)
{
    TSharedRef<FJsonObject> Vector = MakeShared<FJsonObject>();
    Vector->SetNumberField(TEXT("x"), X);
    Vector->SetNumberField(TEXT("y"), Y);
    Vector->SetNumberField(TEXT("z"), Z);
    return Vector;
}

TSharedRef<FJsonObject> MakeQuaternion(
    const double X,
    const double Y,
    const double Z,
    const double W
)
{
    TSharedRef<FJsonObject> Quaternion = MakeShared<FJsonObject>();
    Quaternion->SetNumberField(TEXT("x"), X);
    Quaternion->SetNumberField(TEXT("y"), Y);
    Quaternion->SetNumberField(TEXT("z"), Z);
    Quaternion->SetNumberField(TEXT("w"), W);
    return Quaternion;
}

TArray<TSharedPtr<FJsonValue>> MakeStringArray(
    const TArray<FString>& Values
)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const FString& Value : Values)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

TArray<TSharedPtr<FJsonValue>> MakeNumberArray(
    const TArray<double>& Values
)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const double Value : Values)
    {
        Result.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return Result;
}
}

UExcavatorROSBridgeComponent::UExcavatorROSBridgeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UExcavatorROSBridgeComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bConnectOnBeginPlay)
    {
        Connect();
    }
}

void UExcavatorROSBridgeComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    Disconnect();
    Super::EndPlay(EndPlayReason);
}

void UExcavatorROSBridgeComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const double Now = World->GetTimeSeconds();
    if (
        !IsConnected()
        && !bIntentionalDisconnect
        && bConnectOnBeginPlay
        && Now >= NextReconnectTimeSeconds
    )
    {
        Connect();
    }

    if (
        IsConnected()
        && LastCommandTimeSeconds >= 0.0
        && Now - LastCommandTimeSeconds > CommandWatchdogSeconds
        && !bCommandTimedOut
    )
    {
        FExcavatorNormalizedCommand StopCommand;
        StopCommand.bEmergencyStop = bEmergencyStopLatched;
        DispatchCommand(StopCommand);
        LastCommandTimeSeconds = -1.0;
        SetCommandTimedOut(true);
    }

    if (
        IsConnected()
        && StatePublishRateHz > 0.0f
        && Now >= NextStatePublishTimeSeconds
    )
    {
        PublishState(LastAppliedCommand);
        PublishOperatorState();
        NextStatePublishTimeSeconds =
            Now + 1.0 / static_cast<double>(StatePublishRateHz);
    }
}

void UExcavatorROSBridgeComponent::Connect()
{
    if (Socket.IsValid() && Socket->IsConnected())
    {
        return;
    }

    bIntentionalDisconnect = false;
    Socket.Reset();
    Socket = FWebSocketsModule::Get().CreateWebSocket(ServerUrl);
    Socket->OnConnected().AddUObject(
        this,
        &UExcavatorROSBridgeComponent::HandleConnected
    );
    Socket->OnConnectionError().AddUObject(
        this,
        &UExcavatorROSBridgeComponent::HandleConnectionError
    );
    Socket->OnClosed().AddUObject(
        this,
        &UExcavatorROSBridgeComponent::HandleClosed
    );
    Socket->OnMessage().AddUObject(
        this,
        &UExcavatorROSBridgeComponent::HandleMessage
    );

    UE_LOG(
        LogExcavatorROSBridge,
        Log,
        TEXT("Connecting to rosbridge at %s"),
        *ServerUrl
    );
    Socket->Connect();
}

void UExcavatorROSBridgeComponent::Disconnect()
{
    bIntentionalDisconnect = true;
    if (Socket.IsValid())
    {
        Socket->Close(1000, TEXT("Unreal component stopped"));
        Socket.Reset();
    }
}

bool UExcavatorROSBridgeComponent::IsConnected() const
{
    return Socket.IsValid() && Socket->IsConnected();
}

void UExcavatorROSBridgeComponent::HandleConnected()
{
    UE_LOG(LogExcavatorROSBridge, Log, TEXT("Connected to rosbridge"));
    LastCommandTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    NextStatePublishTimeSeconds = 0.0;
    SetCommandTimedOut(false);
    OnConnectionChanged.Broadcast(true);
    SendSubscriptions();

    AdvertiseTopic(
        ExcavatorROSTopics::State,
        TEXT("excavator_msgs/msg/ExcavatorState")
    );
    AdvertiseTopic(
        ExcavatorROSTopics::JointStates,
        TEXT("sensor_msgs/msg/JointState")
    );
    AdvertiseTopic(
        ExcavatorROSTopics::Transforms,
        TEXT("tf2_msgs/msg/TFMessage")
    );
    AdvertiseTopic(
        ExcavatorROSTopics::LidarScan,
        TEXT("sensor_msgs/msg/LaserScan")
    );
    AdvertiseTopic(
        ExcavatorROSTopics::CameraActive,
        TEXT("std_msgs/msg/String")
    );
    AdvertiseTopic(
        ExcavatorROSTopics::CameraImage,
        TEXT("sensor_msgs/msg/CompressedImage")
    );
    AdvertiseTopic(
        ExcavatorROSTopics::OperatorState,
        TEXT("std_msgs/msg/Bool")
    );
    PublishOperatorState();
}

void UExcavatorROSBridgeComponent::HandleConnectionError(
    const FString& Error
)
{
    UE_LOG(
        LogExcavatorROSBridge,
        Warning,
        TEXT("rosbridge connection error: %s"),
        *Error
    );
    OnConnectionChanged.Broadcast(false);
    const UWorld* World = GetWorld();
    NextReconnectTimeSeconds =
        (World ? World->GetTimeSeconds() : 0.0) + ReconnectDelaySeconds;
}

void UExcavatorROSBridgeComponent::HandleClosed(
    const int32 StatusCode,
    const FString& Reason,
    const bool bWasClean
)
{
    UE_LOG(
        LogExcavatorROSBridge,
        Warning,
        TEXT("rosbridge closed: status=%d clean=%s reason=%s"),
        StatusCode,
        bWasClean ? TEXT("true") : TEXT("false"),
        *Reason
    );
    OnConnectionChanged.Broadcast(false);
    SetCommandTimedOut(true);
    const UWorld* World = GetWorld();
    NextReconnectTimeSeconds =
        (World ? World->GetTimeSeconds() : 0.0) + ReconnectDelaySeconds;
}

void UExcavatorROSBridgeComponent::HandleMessage(const FString& Message)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
        Message
    );
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(
            LogExcavatorROSBridge,
            Warning,
            TEXT("Ignored invalid rosbridge JSON")
        );
        return;
    }

    FString Operation;
    FString Topic;
    if (
        !Root->TryGetStringField(TEXT("op"), Operation)
        || Operation != TEXT("publish")
        || !Root->TryGetStringField(TEXT("topic"), Topic)
        || !Root->HasTypedField<EJson::Object>(TEXT("msg"))
    )
    {
        return;
    }

    const TSharedPtr<FJsonObject> Payload = Root->GetObjectField(TEXT("msg"));
    if (Topic == ExcavatorROSTopics::CameraSelect)
    {
        double Requested = 0.0;
        if (Payload->TryGetNumberField(TEXT("data"), Requested))
        {
            RequestedCameraView = FMath::Clamp(
                FMath::RoundToInt(Requested),
                0,
                4
            );
        }
        return;
    }
    if (Topic == ExcavatorROSTopics::OperatorToggle)
    {
        bool bRequested = false;
        if (
            Payload->TryGetBoolField(TEXT("data"), bRequested)
            && bRequested
        )
        {
            DispatchOperatorToggle();
        }
        return;
    }

    FExcavatorNormalizedCommand Command;

    if (Topic == ExcavatorROSTopics::Command)
    {
        Command.Throttle = ReadNormalizedNumber(Payload, TEXT("throttle"));
        Command.Steering = ReadNormalizedNumber(Payload, TEXT("steering"));
        Command.Swing = ReadNormalizedNumber(Payload, TEXT("swing"));
        Command.Boom = ReadNormalizedNumber(Payload, TEXT("boom"));
        Command.Stick = ReadNormalizedNumber(Payload, TEXT("stick"));
        Command.Bucket = ReadNormalizedNumber(Payload, TEXT("bucket"));
        Payload->TryGetBoolField(
            TEXT("emergency_stop"),
            Command.bEmergencyStop
        );
        Payload->TryGetBoolField(
            TEXT("clear_emergency_stop"),
            Command.bClearEmergencyStop
        );
        Payload->TryGetBoolField(
            TEXT("reset_machine"),
            Command.bResetMachine
        );
    }
    else if (Topic == ExcavatorROSTopics::Velocity)
    {
        if (Payload->HasTypedField<EJson::Object>(TEXT("linear")))
        {
            Command.Throttle = ReadNormalizedNumber(
                Payload->GetObjectField(TEXT("linear")),
                TEXT("x")
            );
        }
        if (Payload->HasTypedField<EJson::Object>(TEXT("angular")))
        {
            Command.Steering = ReadNormalizedNumber(
                Payload->GetObjectField(TEXT("angular")),
                TEXT("z")
            );
        }
    }
    else if (Topic == ExcavatorROSTopics::EmergencyStop)
    {
        Payload->TryGetBoolField(TEXT("data"), Command.bEmergencyStop);
    }
    else
    {
        return;
    }

    DispatchCommand(Command);
}

void UExcavatorROSBridgeComponent::SendSubscriptions()
{
    const auto Subscribe = [this](
        const FString& Topic,
        const FString& Type
    )
    {
        TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
        Request->SetStringField(TEXT("op"), TEXT("subscribe"));
        Request->SetStringField(TEXT("topic"), Topic);
        Request->SetStringField(TEXT("type"), Type);
        Request->SetNumberField(TEXT("queue_length"), 1);
        Request->SetNumberField(TEXT("throttle_rate"), 0);
        SendJson(Request);
    };

    Subscribe(
        ExcavatorROSTopics::Command,
        TEXT("excavator_msgs/msg/ExcavatorCommand")
    );
    Subscribe(
        ExcavatorROSTopics::Velocity,
        TEXT("geometry_msgs/msg/Twist")
    );
    Subscribe(
        ExcavatorROSTopics::EmergencyStop,
        TEXT("std_msgs/msg/Bool")
    );
    Subscribe(
        ExcavatorROSTopics::CameraSelect,
        TEXT("std_msgs/msg/UInt8")
    );
    Subscribe(
        ExcavatorROSTopics::OperatorToggle,
        TEXT("std_msgs/msg/Bool")
    );
}

void UExcavatorROSBridgeComponent::AdvertiseTopic(
    const FString& Topic,
    const FString& Type
)
{
    TSharedRef<FJsonObject> Advertise = MakeShared<FJsonObject>();
    Advertise->SetStringField(TEXT("op"), TEXT("advertise"));
    Advertise->SetStringField(TEXT("topic"), Topic);
    Advertise->SetStringField(TEXT("type"), Type);
    SendJson(Advertise);
}

void UExcavatorROSBridgeComponent::SendJson(
    const TSharedRef<FJsonObject>& Message
)
{
    if (!IsConnected())
    {
        return;
    }

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Serialized);
    if (FJsonSerializer::Serialize(Message, Writer))
    {
        Socket->Send(Serialized);
    }
}

void UExcavatorROSBridgeComponent::DispatchCommand(
    FExcavatorNormalizedCommand Command
)
{
    if (!IsInGameThread())
    {
        const TWeakObjectPtr<UExcavatorROSBridgeComponent> WeakThis(this);
        AsyncTask(
            ENamedThreads::GameThread,
            [WeakThis, Command]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->DispatchCommand(Command);
                }
            }
        );
        return;
    }

    if (Command.bEmergencyStop)
    {
        bEmergencyStopLatched = true;
    }
    if (Command.bClearEmergencyStop)
    {
        bEmergencyStopLatched = false;
    }

    if (bEmergencyStopLatched)
    {
        Command.Throttle = 0.0f;
        Command.Steering = 0.0f;
        Command.Swing = 0.0f;
        Command.Boom = 0.0f;
        Command.Stick = 0.0f;
        Command.Bucket = 0.0f;
        Command.bEmergencyStop = true;
    }

    LastAppliedCommand = Command;

    if (const UWorld* World = GetWorld())
    {
        LastCommandTimeSeconds = World->GetTimeSeconds();
    }
    SetCommandTimedOut(false);

    AActor* Owner = GetOwner();
    bool bWasApplied = false;
    if (
        Owner
        && Owner->GetClass()->ImplementsInterface(
            UExcavatorControlInterface::StaticClass()
        )
    )
    {
        IExcavatorControlInterface::Execute_ApplyExcavatorCommand(
            Owner,
            Command
        );
        bWasApplied = true;
    }

    if (Owner && !bWasApplied)
    {
        TArray<UActorComponent*> Components;
        Owner->GetComponents(Components);
        for (UActorComponent* Component : Components)
        {
            if (
                Component
                && Component != this
                && Component->GetClass()->ImplementsInterface(
                    UExcavatorControlInterface::StaticClass()
                )
            )
            {
                IExcavatorControlInterface::Execute_ApplyExcavatorCommand(
                    Component,
                    Command
                );
                bWasApplied = true;
            }
        }
    }
    OnCommandReceived.Broadcast(Command);
}

void UExcavatorROSBridgeComponent::DispatchOperatorToggle()
{
    if (!IsInGameThread())
    {
        const TWeakObjectPtr<UExcavatorROSBridgeComponent> WeakThis(this);
        AsyncTask(
            ENamedThreads::GameThread,
            [WeakThis]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->DispatchOperatorToggle();
                }
            }
        );
        return;
    }

    ++OperatorToggleGeneration;
    UE_LOG(
        LogExcavatorROSBridge,
        Log,
        TEXT("Received remote operator-mode toggle")
    );
}

void UExcavatorROSBridgeComponent::SetOperatorOnFoot(
    const bool bOnFoot
)
{
    bOperatorOnFoot = bOnFoot;
    PublishOperatorState();
}

void UExcavatorROSBridgeComponent::PublishOperatorState()
{
    if (!IsConnected())
    {
        return;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetBoolField(TEXT("data"), bOperatorOnFoot);
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("op"), TEXT("publish"));
    Root->SetStringField(
        TEXT("topic"),
        ExcavatorROSTopics::OperatorState
    );
    Root->SetObjectField(TEXT("msg"), Payload);
    SendJson(Root);
}

void UExcavatorROSBridgeComponent::SetCommandTimedOut(
    const bool bNewTimedOut
)
{
    if (bCommandTimedOut == bNewTimedOut)
    {
        return;
    }
    bCommandTimedOut = bNewTimedOut;
    OnCommandTimeoutChanged.Broadcast(bCommandTimedOut);
}

float UExcavatorROSBridgeComponent::ReadNormalizedNumber(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FieldName
)
{
    if (!Object.IsValid())
    {
        return 0.0f;
    }

    double Value = 0.0;
    if (!Object->TryGetNumberField(FieldName, Value) || !FMath::IsFinite(Value))
    {
        return 0.0f;
    }
    return FMath::Clamp(static_cast<float>(Value), -1.0f, 1.0f);
}

void UExcavatorROSBridgeComponent::PublishState(
    const FExcavatorNormalizedCommand& CurrentCommand
)
{
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(
        TEXT("header"),
        MakeROSHeader(TEXT("base_link"))
    );
    Payload->SetBoolField(TEXT("ros_connected"), IsConnected());
    Payload->SetBoolField(TEXT("command_timed_out"), bCommandTimedOut);
    Payload->SetBoolField(TEXT("emergency_stop"), bEmergencyStopLatched);
    Payload->SetNumberField(TEXT("throttle"), CurrentCommand.Throttle);
    Payload->SetNumberField(TEXT("steering"), CurrentCommand.Steering);
    Payload->SetNumberField(TEXT("swing"), CurrentCommand.Swing);
    Payload->SetNumberField(TEXT("boom"), CurrentCommand.Boom);
    Payload->SetNumberField(TEXT("stick"), CurrentCommand.Stick);
    Payload->SetNumberField(TEXT("bucket"), CurrentCommand.Bucket);

    float CabDegrees = 0.0f;
    float BoomDegrees = 0.0f;
    float StickDegrees = 0.0f;
    float BucketDegrees = 0.0f;
    if (const AActor* Owner = GetOwner())
    {
        if (
            const UExcavatorVendorAdapterComponent* Adapter =
                Owner->FindComponentByClass<
                    UExcavatorVendorAdapterComponent
                >()
        )
        {
            Adapter->GetJointStateDegrees(
                CabDegrees,
                BoomDegrees,
                StickDegrees,
                BucketDegrees
            );
        }
    }
    Payload->SetNumberField(
        TEXT("cab_yaw"),
        -FMath::DegreesToRadians(CabDegrees)
    );
    Payload->SetNumberField(
        TEXT("boom_angle"),
        FMath::DegreesToRadians(BoomDegrees)
    );
    Payload->SetNumberField(
        TEXT("stick_angle"),
        FMath::DegreesToRadians(StickDegrees)
    );
    Payload->SetNumberField(
        TEXT("bucket_angle"),
        FMath::DegreesToRadians(BucketDegrees)
    );

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("op"), TEXT("publish"));
    Root->SetStringField(TEXT("topic"), ExcavatorROSTopics::State);
    Root->SetObjectField(TEXT("msg"), Payload);
    SendJson(Root);

    TSharedRef<FJsonObject> JointPayload = MakeShared<FJsonObject>();
    JointPayload->SetObjectField(
        TEXT("header"),
        MakeROSHeader(TEXT("base_link"))
    );
    JointPayload->SetArrayField(
        TEXT("name"),
        MakeStringArray(
            {
                TEXT("cab_swing_joint"),
                TEXT("boom_joint"),
                TEXT("stick_joint"),
                TEXT("bucket_joint")
            }
        )
    );
    JointPayload->SetArrayField(
        TEXT("position"),
        MakeNumberArray(
            {
                -FMath::DegreesToRadians(CabDegrees),
                FMath::DegreesToRadians(BoomDegrees),
                FMath::DegreesToRadians(StickDegrees),
                FMath::DegreesToRadians(BucketDegrees)
            }
        )
    );
    JointPayload->SetArrayField(
        TEXT("velocity"),
        TArray<TSharedPtr<FJsonValue>>()
    );
    JointPayload->SetArrayField(
        TEXT("effort"),
        TArray<TSharedPtr<FJsonValue>>()
    );
    TSharedRef<FJsonObject> JointRoot = MakeShared<FJsonObject>();
    JointRoot->SetStringField(TEXT("op"), TEXT("publish"));
    JointRoot->SetStringField(
        TEXT("topic"),
        ExcavatorROSTopics::JointStates
    );
    JointRoot->SetObjectField(TEXT("msg"), JointPayload);
    SendJson(JointRoot);

    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    const FTransform ActorTransform = Owner->GetActorTransform();
    const FVector UnrealLocation = ActorTransform.GetLocation();
    const FQuat UnrealRotation = ActorTransform.GetRotation();
    FQuat ROSRotation(
        -UnrealRotation.X,
        UnrealRotation.Y,
        -UnrealRotation.Z,
        UnrealRotation.W
    );
    ROSRotation.Normalize();

    TSharedRef<FJsonObject> Transform = MakeShared<FJsonObject>();
    Transform->SetObjectField(
        TEXT("translation"),
        MakeVector(
            UnrealLocation.X / 100.0,
            -UnrealLocation.Y / 100.0,
            UnrealLocation.Z / 100.0
        )
    );
    Transform->SetObjectField(
        TEXT("rotation"),
        MakeQuaternion(
            ROSRotation.X,
            ROSRotation.Y,
            ROSRotation.Z,
            ROSRotation.W
        )
    );
    TSharedRef<FJsonObject> StampedTransform = MakeShared<FJsonObject>();
    StampedTransform->SetObjectField(
        TEXT("header"),
        MakeROSHeader(TEXT("map"))
    );
    StampedTransform->SetStringField(
        TEXT("child_frame_id"),
        TEXT("base_link")
    );
    StampedTransform->SetObjectField(TEXT("transform"), Transform);
    TSharedRef<FJsonObject> TFPayload = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Transforms;
    Transforms.Add(MakeShared<FJsonValueObject>(StampedTransform));

    // robot_state_publisher owns the four primary links. Publish the actual
    // animated hydraulic pin positions under unique frames so RViz can draw
    // telescoping cylinders and the bucket linkage without pretending those
    // mechanisms are rigidly attached to one primary link.
    if (const USkeletalMeshComponent* VendorMesh =
            Owner->FindComponentByClass<USkeletalMeshComponent>())
    {
        const struct
        {
            const TCHAR* FrameName;
            const TCHAR* BoneName;
        } BoneFrames[] = {
            // Publish the Marketplace asset's exact animated link pivots.
            // robot_state_publisher still consumes /joint_states for ROS
            // semantics, but its approximate moving TF chain is remapped
            // away from /tf.  RViz therefore draws the meshes at the same
            // transforms Unreal actually uses.
            {
                TEXT("cab_link"),
                TEXT("B_ConstractionExcavator01_Body")
            },
            {
                TEXT("boom_link"),
                TEXT("B_ConstractionExcavator01_Arm02")
            },
            {
                TEXT("stick_link"),
                TEXT("B_ConstractionExcavator01_Arm04")
            },
            {
                TEXT("bucket_link"),
                TEXT("B_ConstractionExcavator01_End")
            },
            {
                TEXT("hyd_01_lower_pin"),
                TEXT("B_ConstractionExcavator01_Hyd01D")
            },
            {
                TEXT("hyd_01_upper_pin"),
                TEXT("B_ConstractionExcavator01_Hyd01U")
            },
            {
                TEXT("hyd_02_lower_pin"),
                TEXT("B_ConstractionExcavator01_Hyd02D")
            },
            {
                TEXT("hyd_02_upper_pin"),
                TEXT("B_ConstractionExcavator01_Hyd02U")
            },
            {
                TEXT("hyd_03_lower_pin"),
                TEXT("B_ConstractionExcavator01_Hyd03D")
            },
            {
                TEXT("hyd_03_upper_pin"),
                TEXT("B_ConstractionExcavator01_Hyd03U")
            },
            {
                TEXT("hyd_04_lower_pin"),
                TEXT("B_ConstractionExcavator01_Hyd04D")
            },
            {
                TEXT("hyd_04_upper_pin"),
                TEXT("B_ConstractionExcavator01_Hyd04U")
            },
            {
                TEXT("hyd_05_lower_pin"),
                TEXT("B_ConstractionExcavator01_Hyd05D")
            },
            {
                TEXT("hyd_05_upper_pin"),
                TEXT("B_ConstractionExcavator01_Hyd05U")
            },
            {
                TEXT("bucket_linkage_end"),
                TEXT("B_ConstractionExcavator01_End")
            },
            {
                TEXT("bucket_linkage_end01"),
                TEXT("B_ConstractionExcavator01_End01")
            },
            {
                TEXT("bucket_linkage_end03"),
                TEXT("B_ConstractionExcavator01_End03")
            }
        };

        for (const auto& BoneFrame : BoneFrames)
        {
            const FName BoneName(BoneFrame.BoneName);
            if (VendorMesh->GetBoneIndex(BoneName) == INDEX_NONE)
            {
                continue;
            }
            const FTransform BoneWorld =
                VendorMesh->GetSocketTransform(BoneName, RTS_World);
            const FTransform Relative =
                BoneWorld.GetRelativeTransform(ActorTransform);
            const FVector Position = Relative.GetLocation();
            const FQuat UnrealBoneRotation = Relative.GetRotation();
            FQuat ROSBoneRotation(
                -UnrealBoneRotation.X,
                UnrealBoneRotation.Y,
                -UnrealBoneRotation.Z,
                UnrealBoneRotation.W
            );
            ROSBoneRotation.Normalize();

            TSharedRef<FJsonObject> BoneTransform =
                MakeShared<FJsonObject>();
            BoneTransform->SetObjectField(
                TEXT("translation"),
                MakeVector(
                    Position.X / 100.0,
                    -Position.Y / 100.0,
                    Position.Z / 100.0
                )
            );
            BoneTransform->SetObjectField(
                TEXT("rotation"),
                MakeQuaternion(
                    ROSBoneRotation.X,
                    ROSBoneRotation.Y,
                    ROSBoneRotation.Z,
                    ROSBoneRotation.W
                )
            );
            TSharedRef<FJsonObject> BoneStamped =
                MakeShared<FJsonObject>();
            BoneStamped->SetObjectField(
                TEXT("header"),
                MakeROSHeader(TEXT("base_link"))
            );
            BoneStamped->SetStringField(
                TEXT("child_frame_id"),
                BoneFrame.FrameName
            );
            BoneStamped->SetObjectField(
                TEXT("transform"),
                BoneTransform
            );
            Transforms.Add(
                MakeShared<FJsonValueObject>(BoneStamped)
            );
        }

        // Publish the same calibrated cutting edge used by DiggableTerrain.
        // The dashboard can then draw the physical bucket mouth instead of
        // guessing its direction from one of the four-bar linkage pins.
        const FName BucketBone(
            TEXT("B_ConstractionExcavator01_End")
        );
        if (VendorMesh->GetBoneIndex(BucketBone) != INDEX_NONE)
        {
            const FTransform BucketWorld =
                VendorMesh->GetSocketTransform(
                    BucketBone,
                    RTS_World
                );
            const FVector CuttingEdgeWorld =
                BucketWorld.TransformPosition(
                    FVector(-27.0f, 0.0f, 88.32f)
                );
            const FVector CuttingEdgeRelative =
                ActorTransform.InverseTransformPosition(
                    CuttingEdgeWorld
                );
            const FQuat UnrealBucketRotation =
                BucketWorld
                    .GetRelativeTransform(ActorTransform)
                    .GetRotation();
            FQuat ROSBucketRotation(
                -UnrealBucketRotation.X,
                UnrealBucketRotation.Y,
                -UnrealBucketRotation.Z,
                UnrealBucketRotation.W
            );
            ROSBucketRotation.Normalize();

            TSharedRef<FJsonObject> CuttingEdgeTransform =
                MakeShared<FJsonObject>();
            CuttingEdgeTransform->SetObjectField(
                TEXT("translation"),
                MakeVector(
                    CuttingEdgeRelative.X / 100.0,
                    -CuttingEdgeRelative.Y / 100.0,
                    CuttingEdgeRelative.Z / 100.0
                )
            );
            CuttingEdgeTransform->SetObjectField(
                TEXT("rotation"),
                MakeQuaternion(
                    ROSBucketRotation.X,
                    ROSBucketRotation.Y,
                    ROSBucketRotation.Z,
                    ROSBucketRotation.W
                )
            );

            TSharedRef<FJsonObject> CuttingEdgeStamped =
                MakeShared<FJsonObject>();
            CuttingEdgeStamped->SetObjectField(
                TEXT("header"),
                MakeROSHeader(TEXT("base_link"))
            );
            CuttingEdgeStamped->SetStringField(
                TEXT("child_frame_id"),
                TEXT("bucket_cutting_edge")
            );
            CuttingEdgeStamped->SetObjectField(
                TEXT("transform"),
                CuttingEdgeTransform
            );
            Transforms.Add(
                MakeShared<FJsonValueObject>(CuttingEdgeStamped)
            );
        }
    }

    // Publish the sensor frame from the exact transform used for the Unreal
    // ray scan. A fixed URDF offset is not safe here because the Marketplace
    // cab bone uses a rotated authoring coordinate system.
    if (
        const UExcavatorSensorRigComponent* SensorRig =
            Owner->FindComponentByClass<UExcavatorSensorRigComponent>()
    )
    {
        FTransform LidarWorld;
        if (SensorRig->GetLidarWorldTransform(LidarWorld))
        {
            const FTransform LidarRelative =
                LidarWorld.GetRelativeTransform(ActorTransform);
            const FVector LidarPosition =
                LidarRelative.GetLocation();
            const FQuat UnrealLidarRotation =
                LidarRelative.GetRotation();
            FQuat ROSLidarRotation(
                -UnrealLidarRotation.X,
                UnrealLidarRotation.Y,
                -UnrealLidarRotation.Z,
                UnrealLidarRotation.W
            );
            ROSLidarRotation.Normalize();

            TSharedRef<FJsonObject> LidarTransform =
                MakeShared<FJsonObject>();
            LidarTransform->SetObjectField(
                TEXT("translation"),
                MakeVector(
                    LidarPosition.X / 100.0,
                    -LidarPosition.Y / 100.0,
                    LidarPosition.Z / 100.0
                )
            );
            LidarTransform->SetObjectField(
                TEXT("rotation"),
                MakeQuaternion(
                    ROSLidarRotation.X,
                    ROSLidarRotation.Y,
                    ROSLidarRotation.Z,
                    ROSLidarRotation.W
                )
            );

            TSharedRef<FJsonObject> LidarStamped =
                MakeShared<FJsonObject>();
            LidarStamped->SetObjectField(
                TEXT("header"),
                MakeROSHeader(TEXT("base_link"))
            );
            LidarStamped->SetStringField(
                TEXT("child_frame_id"),
                TEXT("lidar_link")
            );
            LidarStamped->SetObjectField(
                TEXT("transform"),
                LidarTransform
            );
            Transforms.Add(
                MakeShared<FJsonValueObject>(LidarStamped)
            );
        }
    }

    TFPayload->SetArrayField(TEXT("transforms"), Transforms);
    TSharedRef<FJsonObject> TFRoot = MakeShared<FJsonObject>();
    TFRoot->SetStringField(TEXT("op"), TEXT("publish"));
    TFRoot->SetStringField(
        TEXT("topic"),
        ExcavatorROSTopics::Transforms
    );
    TFRoot->SetObjectField(TEXT("msg"), TFPayload);
    SendJson(TFRoot);
}

void UExcavatorROSBridgeComponent::PublishLaserScan(
    const TArray<float>& RangesMeters,
    const float AngleMinimumRadians,
    const float AngleMaximumRadians,
    const float RangeMinimumMeters,
    const float RangeMaximumMeters,
    const float ScanTimeSeconds,
    const FString& FrameId
)
{
    if (!IsConnected() || RangesMeters.IsEmpty())
    {
        return;
    }

    TArray<double> Ranges;
    Ranges.Reserve(RangesMeters.Num());
    for (const float Range : RangesMeters)
    {
        Ranges.Add(Range);
    }
    const float AngleIncrement =
        (AngleMaximumRadians - AngleMinimumRadians)
        / static_cast<float>(RangesMeters.Num());

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("header"), MakeROSHeader(FrameId));
    Payload->SetNumberField(TEXT("angle_min"), AngleMinimumRadians);
    Payload->SetNumberField(TEXT("angle_max"), AngleMaximumRadians);
    Payload->SetNumberField(TEXT("angle_increment"), AngleIncrement);
    Payload->SetNumberField(
        TEXT("time_increment"),
        ScanTimeSeconds / static_cast<float>(RangesMeters.Num())
    );
    Payload->SetNumberField(TEXT("scan_time"), ScanTimeSeconds);
    Payload->SetNumberField(TEXT("range_min"), RangeMinimumMeters);
    Payload->SetNumberField(TEXT("range_max"), RangeMaximumMeters);
    Payload->SetArrayField(TEXT("ranges"), MakeNumberArray(Ranges));
    Payload->SetArrayField(
        TEXT("intensities"),
        TArray<TSharedPtr<FJsonValue>>()
    );

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("op"), TEXT("publish"));
    Root->SetStringField(
        TEXT("topic"),
        ExcavatorROSTopics::LidarScan
    );
    Root->SetObjectField(TEXT("msg"), Payload);
    SendJson(Root);
}

void UExcavatorROSBridgeComponent::PublishActiveCamera(
    const int32 ViewIndex,
    const FString& ViewName
)
{
    if (!IsConnected())
    {
        return;
    }
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(
        TEXT("data"),
        FString::Printf(TEXT("%d:%s"), ViewIndex, *ViewName)
    );
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("op"), TEXT("publish"));
    Root->SetStringField(
        TEXT("topic"),
        ExcavatorROSTopics::CameraActive
    );
    Root->SetObjectField(TEXT("msg"), Payload);
    SendJson(Root);
}

void UExcavatorROSBridgeComponent::PublishCompressedCameraImage(
    const uint8* CompressedData,
    const uint32 DataSize,
    const FString& Format,
    const FString& FrameId
)
{
    if (
        !IsConnected()
        || CompressedData == nullptr
        || DataSize == 0
    )
    {
        return;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("header"), MakeROSHeader(FrameId));
    Payload->SetStringField(TEXT("format"), Format);
    Payload->SetStringField(
        TEXT("data"),
        FBase64::Encode(CompressedData, DataSize)
    );

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("op"), TEXT("publish"));
    Root->SetStringField(
        TEXT("topic"),
        ExcavatorROSTopics::CameraImage
    );
    Root->SetObjectField(TEXT("msg"), Payload);
    SendJson(Root);
}
