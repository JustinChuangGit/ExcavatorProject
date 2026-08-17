#include "DiggableTerrain.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "ExcavatorOperatorCharacter.h"
#include "ExcavatorROSBridgeComponent.h"
#include "ExcavatorSensorRigComponent.h"
#include "ExcavatorVendorAdapterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDiggableTerrain, Log, All);

ADiggableTerrain::ADiggableTerrain()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.0f;

    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(
        TEXT("DiggableMarsSoil")
    );
    SetRootComponent(TerrainMesh);
    TerrainMesh->SetMobility(EComponentMobility::Movable);
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TerrainMesh->SetCastShadow(true);

    // Runtime collision is split into small tiles below. This compatibility
    // component supplies editor collision before play begins.
    StableCollisionMesh = CreateDefaultSubobject<UProceduralMeshComponent>(
        TEXT("StableMarsDrivingCollision")
    );
    StableCollisionMesh->SetupAttachment(TerrainMesh);
    StableCollisionMesh->SetMobility(EComponentMobility::Movable);
    StableCollisionMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics
    );
    StableCollisionMesh->SetCollisionObjectType(ECC_WorldStatic);
    StableCollisionMesh->SetCollisionResponseToAllChannels(ECR_Block);
    StableCollisionMesh->bUseComplexAsSimpleCollision = true;
    StableCollisionMesh->bUseAsyncCooking = true;
    StableCollisionMesh->SetCastShadow(false);
    StableCollisionMesh->SetVisibility(false, true);
    StableCollisionMesh->SetHiddenInGame(true);

    BucketLoadMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CarriedRegolith")
    );
    BucketLoadMesh->SetupAttachment(TerrainMesh);
    BucketLoadMesh->SetMobility(EComponentMobility::Movable);
    BucketLoadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BucketLoadMesh->SetCastShadow(true);
    BucketLoadMesh->SetHiddenInGame(true);
    BucketLoadMesh->SetVisibility(false, true);

    BucketSoilMesh = CreateDefaultSubobject<UProceduralMeshComponent>(
        TEXT("BucketShapedRegolith")
    );
    BucketSoilMesh->SetupAttachment(TerrainMesh);
    BucketSoilMesh->SetMobility(EComponentMobility::Movable);
    BucketSoilMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BucketSoilMesh->SetCastShadow(true);
    BucketSoilMesh->SetHiddenInGame(true);
    BucketSoilMesh->SetVisibility(false, true);

    GranularSoilInstances =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(
            TEXT("GranularRegolithParticles")
        );
    GranularSoilInstances->SetupAttachment(TerrainMesh);
    GranularSoilInstances->SetMobility(EComponentMobility::Movable);
    GranularSoilInstances->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    GranularSoilInstances->SetCastShadow(false);
    GranularSoilInstances->SetCanEverAffectNavigation(false);
    GranularSoilInstances->SetCullDistances(0, 5000);

    DumpStreamInstances =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(
            TEXT("RegolithPourStream")
        );
    DumpStreamInstances->SetupAttachment(TerrainMesh);
    DumpStreamInstances->SetMobility(EComponentMobility::Movable);
    DumpStreamInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DumpStreamInstances->SetCastShadow(false);
    DumpStreamInstances->SetCanEverAffectNavigation(false);
    DumpStreamInstances->SetCullDistances(0, 5500);
    DumpStreamInstances->SetHiddenInGame(true);
    DumpStreamInstances->SetVisibility(false, true);

    SurfaceClumpInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("SleepingRegolithClumps")
        );
    SurfaceClumpInstances->SetupAttachment(TerrainMesh);
    SurfaceClumpInstances->SetMobility(EComponentMobility::Movable);
    SurfaceClumpInstances->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    SurfaceClumpInstances->SetCastShadow(false);
    SurfaceClumpInstances->SetCanEverAffectNavigation(false);
    SurfaceClumpInstances->SetCullDistances(1800, 7000);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ParticleShape(
        TEXT("/Game/StarterContent/Props/SM_Rock.SM_Rock")
    );
    if (ParticleShape.Succeeded())
    {
        GranularSoilInstances->SetStaticMesh(ParticleShape.Object);
        DumpStreamInstances->SetStaticMesh(ParticleShape.Object);
    }
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SurfaceClumpShape(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );
    if (SurfaceClumpShape.Succeeded())
    {
        SurfaceClumpInstances->SetStaticMesh(
            SurfaceClumpShape.Object
        );
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        ParticleMaterial(
            TEXT(
                "/Engine/BasicShapes/"
                "BasicShapeMaterial.BasicShapeMaterial"
            )
        );
    if (ParticleMaterial.Succeeded())
    {
        GranularParticleMaterialBase = ParticleMaterial.Object;
    }
}

void ADiggableTerrain::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateTerrain();
}

void ADiggableTerrain::BeginPlay()
{
    Super::BeginPlay();

    // This simulation is presented through a clean mission-control stream.
    // Keep diagnostic information in the log, never over the operator video.
    if (GEngine)
    {
        GEngine->Exec(
            GetWorld(),
            TEXT("DisableAllScreenMessages")
        );
    }

    // Preserve the approximately 72 m field width, but divide it into 64-cell
    // tiles with a 12.5 cm runtime sample spacing. The former 45 cm spacing
    // provided only one useful sample across the bucket.
    const float TerrainWidth =
        GridResolution * CellSizeCentimeters;
    TileCellCount = FMath::Clamp(TileCellCount, 16, 128);
    const int32 DesiredCellCount = FMath::CeilToInt(
        TerrainWidth
        / FMath::Max(RuntimeTargetCellSizeCentimeters, 8.0f)
    );
    GridResolution = FMath::Clamp(
        FMath::DivideAndRoundUp(DesiredCellCount, TileCellCount)
            * TileCellCount,
        TileCellCount,
        768
    );
    CellSizeCentimeters =
        TerrainWidth / FMath::Max(GridResolution, 1);
    // Calibrated in the live End-bone frame from the separated bucket mesh.
    // The visible bowl centre is about +4 cm in X and +80 cm in Z. The procedural
    // cavity treats local +X as the open mouth, while the Marketplace bucket
    // opens toward End local -X, so rotate 180 degrees about local Y.
    BucketSoilRelativeLocation = FVector(4.0f, 0.0f, 80.0f);
    BucketSoilRelativeRotation = FRotator(180.0f, 0.0f, 0.0f);
    BucketInteriorLengthCentimeters = 44.0f;
    BucketInteriorWidthCentimeters = 48.0f;
    BucketInteriorDepthCentimeters = 26.0f;
    BucketHeapHeightCentimeters = 15.0f;
    BucketCapacityCubicMeters = 0.05f;
    DumpRateCubicMetersPerSecond = 0.10f;
    ContactToleranceCentimeters = 28.0f;
    BucketProbeSpreadCentimeters = 16.0f;
    MaximumBucketPenetrationCentimeters = 28.0f;
    DigRadiusCentimeters = FMath::Clamp(
        BucketInteriorWidthCentimeters * 0.55f,
        20.0f,
        40.0f
    );
    MaximumCutPerStepCentimeters =
        FMath::Max(MaximumCutPerStepCentimeters, 12.0f);
    InteractionIntervalSeconds =
        FMath::Min(InteractionIntervalSeconds, 0.05f);
    GranularParticleRadiusCentimeters = 3.2f;
    GranularPatchLengthCentimeters = 560.0f;
    GranularPatchWidthCentimeters = 400.0f;
    GranularBedDepthCentimeters = 14.0f;
    MaximumGranularParticles = 10000;
    GranularActivationRadiusCentimeters = 175.0f;
    SurfaceClumpSpacingCentimeters = FMath::Max(
        SurfaceClumpSpacingCentimeters,
        58.0f
    );
    GranularContactStiffness = FMath::Max(
        GranularContactStiffness,
        0.72f
    );
    GranularSurfaceFriction = FMath::Max(
        GranularSurfaceFriction,
        0.82f
    );
    // A short scrape should create a partial load, while a deliberate deep
    // curl should take a few seconds of contact to fill the bucket.
    PassiveCaptureEfficiency = 0.04f;
    CurlCaptureEfficiency = 0.40f;
    MaximumBucketFillFractionPerSecond = 0.45f;
    SpoilRetentionRatio = 1.0f;
    bEnableGranularSoil = false;
    bEnableSurfaceClumps = false;

    // Always rebuild from the current runtime profile so maps saved with an
    // earlier class default cannot silently retain the coarse interaction.
    GenerateTerrain();
    InitializeCollisionTiles();
    UE_LOG(
        LogDiggableTerrain,
        Log,
        TEXT(
            "Whole-site soil ready: %dx%d cells at %.1f cm, "
            "%dx%d independently updateable tiles"
        ),
        GridResolution,
        GridResolution,
        CellSizeCentimeters,
        TerrainTileCountPerAxis(),
        TerrainTileCountPerAxis()
    );
    GranularSoilInstances->ClearInstances();
    GranularSoilInstances->SetVisibility(false, true);
    GranularSoilInstances->SetHiddenInGame(true);
    SurfaceClumpInstances->ClearInstances();
    SurfaceClumpInstances->SetVisibility(false, true);
    SurfaceClumpInstances->SetHiddenInGame(true);
    if (SoilMaterial)
    {
        DumpStreamInstances->SetMaterial(0, SoilMaterial);
    }
    if (bSetInitialCameraView)
    {
        if (APlayerController* PlayerController =
                GetWorld()->GetFirstPlayerController())
        {
            PlayerController->SetControlRotation(
                InitialCameraControlRotation
            );
        }
    }
    ResolveExcavatorMesh();
    InitializeGranularSoil();
    InitializeSurfaceClumps();
#if !UE_BUILD_SHIPPING
    const bool bDebugBucketFillRequested =
        FParse::Param(FCommandLine::Get(), TEXT("DebugBucketFill"));
    float RequestedBucketFillPercent = -1.0f;
    const bool bRequestedSpecificBucketFill =
        FParse::Value(
            FCommandLine::Get(),
            TEXT("BucketFillPercent="),
            RequestedBucketFillPercent
        );
    if (
        bDebugBucketFillRequested
        || FParse::Param(FCommandLine::Get(), TEXT("PrefillBucket"))
        || bRequestedSpecificBucketFill
    )
    {
        const float PrefillRatio = bRequestedSpecificBucketFill
            ? FMath::Clamp(
                RequestedBucketFillPercent / 100.0f,
                0.0f,
                1.0f
            )
            : 0.68f;
        CarriedSoilVolumeCubicCentimeters =
            BucketCapacityCubicMeters * 1000000.0f * PrefillRatio;
        UpdateBucketLoadVisual();
        UE_LOG(
            LogDiggableTerrain,
            Log,
            TEXT("Diagnostic bucket prefill: %.1f%%"),
            PrefillRatio * 100.0f
        );
    }
    if (bDebugBucketFillRequested)
    {
        bDebugBucketFill = true;
        PrimaryActorTick.TickGroup = TG_PostUpdateWork;
        if (APlayerController* PlayerController =
                GetWorld()->GetFirstPlayerController())
        {
            ACameraActor* CameraActor =
                GetWorld()->SpawnActor<ACameraActor>();
            if (CameraActor)
            {
                DebugBucketCamera = CameraActor;
                CameraActor->GetCameraComponent()->SetFieldOfView(35.0f);
                PlayerController->SetViewTarget(CameraActor);
            }
        }
    }
    bStartOperatorOnFootRequested =
        FParse::Param(FCommandLine::Get(), TEXT("StartOnFoot"));
#endif

    if (bStartOperatorOnFootRequested)
    {
        ExitExcavator();
        bStartOperatorOnFootRequested = false;
    }
}

void ADiggableTerrain::Tick(const float DeltaTime)
{
    Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
    if (bDebugBucketFill)
    {
        // The vendor Blueprint ignores controller yaw and reapplies its
        // spring-arm transform every frame. Move its active camera directly,
        // after normal actor updates, for this visual-calibration mode only.
        if (ExcavatorMesh.IsValid())
        {
            AActor* Machine = ExcavatorMesh->GetOwner();
            if (ACameraActor* CameraActor =
                    DebugBucketCamera.Get())
            {
                const FTransform MachineTransform =
                    Machine->GetActorTransform();
                const FTransform BucketTransform =
                    ExcavatorMesh->GetSocketTransform(
                        BucketBoneName,
                        RTS_World
                    );
                const FVector Target =
                    BucketTransform.TransformPosition(
                        BucketSoilRelativeLocation
                    );
                const FVector CameraLocation =
                    Target
                    - MachineTransform.GetUnitAxis(EAxis::Y) * 310.0f
                    + FVector::UpVector * 210.0f;
                CameraActor->SetActorLocationAndRotation(
                    CameraLocation,
                    (Target - CameraLocation).Rotation()
                );
            }
        }
        UpdateBucketLoadVisual();
        // Continue through the normal interaction path so a live ROS command
        // can exercise the real scoop and dump code while this close camera is
        // active.
    }
#endif

    ResolveExcavatorMesh();
    UpdateOperatorModeInput();
    UpdateResetInput();
    TickGranularSoil(DeltaTime);
    TickDumpStream(DeltaTime);
    UpdatePendingCollisionTiles(DeltaTime);

    InteractionAccumulator += DeltaTime;
    if (InteractionAccumulator < InteractionIntervalSeconds)
    {
        return;
    }
    InteractionAccumulator = 0.0f;

    UpdateRosReset();
    ProcessBucketInteraction();
}

void ADiggableTerrain::GenerateTerrain()
{
    const float RequestedTerrainWidth =
        FMath::Max(
            GridResolution * CellSizeCentimeters,
            TileCellCount * 8.0f
        );
    TileCellCount = FMath::Clamp(TileCellCount, 16, 128);
    GridResolution = FMath::Clamp(
        FMath::DivideAndRoundUp(
            FMath::Max(GridResolution, TileCellCount),
            TileCellCount
        ) * TileCellCount,
        TileCellCount,
        768
    );
    CellSizeCentimeters = FMath::Max(
        RequestedTerrainWidth / GridResolution,
        8.0f
    );

    BuildTopology();
    const int32 VertexCount =
        (GridResolution + 1) * (GridResolution + 1);
    InitialHeights.SetNumZeroed(VertexCount);
    CurrentHeights.SetNumZeroed(VertexCount);

    const FVector2D Minimum = GridMinimum();
    const float HalfExtent =
        GridResolution * CellSizeCentimeters * 0.5f;
    for (int32 Y = 0; Y <= GridResolution; ++Y)
    {
        for (int32 X = 0; X <= GridResolution; ++X)
        {
            const int32 Index = VertexIndex(X, Y);
            const float LocalX =
                Minimum.X + X * CellSizeCentimeters;
            const float LocalY =
                Minimum.Y + Y * CellSizeCentimeters;
            const float Radius =
                FVector2D(LocalX, LocalY).Size();

            const float BroadNoise = FMath::PerlinNoise2D(
                FVector2D(LocalX, LocalY) * 0.00042f
                + FVector2D(17.3f, -9.1f)
            );
            const float FineNoise = FMath::PerlinNoise2D(
                FVector2D(LocalX, LocalY) * 0.00135f
                + FVector2D(-4.8f, 21.7f)
            );

            // Keep the central work pad comparatively level so the wheeled
            // Marketplace excavator remains stable, then blend into rolling
            // Martian terrain toward the perimeter.
            const float ReliefBlend = FMath::SmoothStep(
                HalfExtent * 0.42f,
                HalfExtent * 0.82f,
                Radius
            );
            const float Height =
                ReliefBlend
                * TerrainReliefCentimeters
                * (BroadNoise * 0.72f + FineNoise * 0.28f);

            InitialHeights[Index] = Height;
            CurrentHeights[Index] = Height;
        }
    }

    TerrainMesh->ClearAllMeshSections();
    StableCollisionMesh->ClearAllMeshSections();
    const int32 TileCount = TerrainTileCountPerAxis();
    for (int32 TileY = 0; TileY < TileCount; ++TileY)
    {
        for (int32 TileX = 0; TileX < TileCount; ++TileX)
        {
            TArray<FVector> TileVertices;
            TArray<int32> TileTriangles;
            TArray<FVector> TileNormals;
            TArray<FVector2D> TileUVs;
            TArray<FLinearColor> TileColors;
            TArray<FProcMeshTangent> TileTangents;
            BuildTerrainTileData(
                TileX,
                TileY,
                TileVertices,
                TileTriangles,
                TileNormals,
                TileUVs,
                TileColors,
                TileTangents
            );
            const int32 SectionIndex = TileY * TileCount + TileX;
            TerrainMesh->CreateMeshSection_LinearColor(
                SectionIndex,
                TileVertices,
                TileTriangles,
                TileNormals,
                TileUVs,
                TileColors,
                TileTangents,
                false,
                false
            );
            StableCollisionMesh->CreateMeshSection_LinearColor(
                SectionIndex,
                TileVertices,
                TileTriangles,
                TileNormals,
                TileUVs,
                TileColors,
                TileTangents,
                true,
                false
            );
            if (SoilMaterial)
            {
                TerrainMesh->SetMaterial(SectionIndex, SoilMaterial);
            }
        }
    }
    StableCollisionMesh->SetVisibility(false, true);
    StableCollisionMesh->SetHiddenInGame(true);
    BucketSoilMesh->SetMaterial(0, SoilMaterial);
    DirtyTerrainTiles.Reset();
    PendingCollisionTiles.Reset();
}

void ADiggableTerrain::BuildTopology()
{
    // Terrain topology is generated per tile. Retire the former monolithic
    // arrays so only changed sections need to be sent to the render thread.
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UVs.Reset();
    VertexColors.Reset();
    Tangents.Reset();
}

void ADiggableTerrain::RebuildDerivedMeshData()
{
    MarkAllTerrainTilesDirty();
}

void ADiggableTerrain::UpdateTerrainMesh()
{
    if (DirtyTerrainTiles.IsEmpty())
    {
        return;
    }

    const int32 TileCount = TerrainTileCountPerAxis();
    for (const int32 SectionIndex : DirtyTerrainTiles)
    {
        if (SectionIndex < 0 || SectionIndex >= TileCount * TileCount)
        {
            continue;
        }
        const int32 TileX = SectionIndex % TileCount;
        const int32 TileY = SectionIndex / TileCount;
        TArray<FVector> TileVertices;
        TArray<int32> TileTriangles;
        TArray<FVector> TileNormals;
        TArray<FVector2D> TileUVs;
        TArray<FLinearColor> TileColors;
        TArray<FProcMeshTangent> TileTangents;
        BuildTerrainTileData(
            TileX,
            TileY,
            TileVertices,
            TileTriangles,
            TileNormals,
            TileUVs,
            TileColors,
            TileTangents
        );
        TerrainMesh->UpdateMeshSection_LinearColor(
            SectionIndex,
            TileVertices,
            TileNormals,
            TileUVs,
            TileColors,
            TileTangents,
            false
        );
        PendingCollisionTiles.Add(SectionIndex);
    }
    DirtyTerrainTiles.Reset();
}

void ADiggableTerrain::BuildTerrainTileData(
    const int32 TileX,
    const int32 TileY,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector>& OutNormals,
    TArray<FVector2D>& OutUVs,
    TArray<FLinearColor>& OutVertexColors,
    TArray<FProcMeshTangent>& OutTangents
) const
{
    const int32 StartX = TileX * TileCellCount;
    const int32 StartY = TileY * TileCellCount;
    const int32 CellsX = FMath::Min(
        TileCellCount,
        GridResolution - StartX
    );
    const int32 CellsY = FMath::Min(
        TileCellCount,
        GridResolution - StartY
    );
    const int32 RowSize = CellsX + 1;
    const int32 VertexCount = RowSize * (CellsY + 1);
    OutVertices.SetNumUninitialized(VertexCount);
    OutNormals.SetNumUninitialized(VertexCount);
    OutUVs.SetNumUninitialized(VertexCount);
    OutVertexColors.SetNumUninitialized(VertexCount);
    OutTangents.SetNumUninitialized(VertexCount);
    OutTriangles.Reset(CellsX * CellsY * 6);

    const FVector2D Minimum = GridMinimum();
    const FLinearColor UndisturbedSoil(1.00f, 0.78f, 0.66f, 1.0f);
    const FLinearColor FreshCutSoil(0.48f, 0.36f, 0.32f, 1.0f);
    const FLinearColor LooseSpoil(1.35f, 1.16f, 0.96f, 1.0f);

    for (int32 LocalY = 0; LocalY <= CellsY; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX <= CellsX; ++LocalX)
        {
            const int32 GridX = StartX + LocalX;
            const int32 GridY = StartY + LocalY;
            const int32 SourceIndex = VertexIndex(GridX, GridY);
            const int32 TileIndex = LocalY * RowSize + LocalX;
            OutVertices[TileIndex] = FVector(
                Minimum.X + GridX * CellSizeCentimeters,
                Minimum.Y + GridY * CellSizeCentimeters,
                CurrentHeights[SourceIndex]
            );
            OutUVs[TileIndex] =
                FVector2D(GridX, GridY) * 0.18f;

            const int32 LeftX = FMath::Max(GridX - 1, 0);
            const int32 RightX = FMath::Min(
                GridX + 1,
                GridResolution
            );
            const int32 DownY = FMath::Max(GridY - 1, 0);
            const int32 UpY = FMath::Min(
                GridY + 1,
                GridResolution
            );
            const float SpanX = FMath::Max(
                (RightX - LeftX) * CellSizeCentimeters,
                1.0f
            );
            const float SpanY = FMath::Max(
                (UpY - DownY) * CellSizeCentimeters,
                1.0f
            );
            const float SlopeX =
                (
                    CurrentHeights[VertexIndex(RightX, GridY)]
                    - CurrentHeights[VertexIndex(LeftX, GridY)]
                ) / SpanX;
            const float SlopeY =
                (
                    CurrentHeights[VertexIndex(GridX, UpY)]
                    - CurrentHeights[VertexIndex(GridX, DownY)]
                ) / SpanY;
            OutNormals[TileIndex] =
                FVector(-SlopeX, -SlopeY, 1.0f).GetSafeNormal();
            OutTangents[TileIndex] = FProcMeshTangent(
                FVector(1.0f, 0.0f, SlopeX).GetSafeNormal(),
                false
            );

            const float Difference =
                CurrentHeights[SourceIndex] - InitialHeights[SourceIndex];
            if (Difference < -2.0f)
            {
                OutVertexColors[TileIndex] = FMath::Lerp(
                    UndisturbedSoil,
                    FreshCutSoil,
                    FMath::Clamp(-Difference / 35.0f, 0.0f, 1.0f)
                );
            }
            else if (Difference > 2.0f)
            {
                OutVertexColors[TileIndex] = FMath::Lerp(
                    UndisturbedSoil,
                    LooseSpoil,
                    FMath::Clamp(Difference / 28.0f, 0.0f, 1.0f)
                );
            }
            else
            {
                const float Fleck =
                    0.88f
                    + 0.12f
                        * FMath::PerlinNoise2D(
                            FVector2D(GridX, GridY) * 0.37f
                        );
                OutVertexColors[TileIndex] = UndisturbedSoil * Fleck;
                OutVertexColors[TileIndex].A = 1.0f;
            }
        }
    }

    for (int32 LocalY = 0; LocalY < CellsY; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < CellsX; ++LocalX)
        {
            const int32 V00 = LocalY * RowSize + LocalX;
            const int32 V10 = V00 + 1;
            const int32 V01 = V00 + RowSize;
            const int32 V11 = V01 + 1;
            OutTriangles.Append({V00, V11, V10, V00, V01, V11});
        }
    }
}

int32 ADiggableTerrain::TerrainTileCountPerAxis() const
{
    return FMath::Max(
        FMath::DivideAndRoundUp(GridResolution, TileCellCount),
        1
    );
}

int32 ADiggableTerrain::TerrainTileIndexForVertex(
    const int32 X,
    const int32 Y
) const
{
    const int32 TileCount = TerrainTileCountPerAxis();
    return
        FMath::Clamp(Y / TileCellCount, 0, TileCount - 1)
            * TileCount
        + FMath::Clamp(X / TileCellCount, 0, TileCount - 1);
}

void ADiggableTerrain::MarkTerrainVertexDirty(
    const int32 X,
    const int32 Y
)
{
    const int32 TileCount = TerrainTileCountPerAxis();
    const int32 CenterTileX =
        FMath::Clamp(X / TileCellCount, 0, TileCount - 1);
    const int32 CenterTileY =
        FMath::Clamp(Y / TileCellCount, 0, TileCount - 1);
    for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
    {
        for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
        {
            const int32 DirtyX = CenterTileX + OffsetX;
            const int32 DirtyY = CenterTileY + OffsetY;
            if (
                DirtyX >= 0
                && DirtyX < TileCount
                && DirtyY >= 0
                && DirtyY < TileCount
            )
            {
                DirtyTerrainTiles.Add(DirtyY * TileCount + DirtyX);
            }
        }
    }
}

void ADiggableTerrain::MarkAllTerrainTilesDirty()
{
    const int32 TileCount = TerrainTileCountPerAxis();
    for (int32 TileIndex = 0;
         TileIndex < TileCount * TileCount;
         ++TileIndex)
    {
        DirtyTerrainTiles.Add(TileIndex);
    }
}

void ADiggableTerrain::InitializeCollisionTiles()
{
    for (UProceduralMeshComponent* ExistingTile : CollisionTileMeshes)
    {
        if (ExistingTile)
        {
            ExistingTile->DestroyComponent();
        }
    }
    CollisionTileMeshes.Reset();
    StableCollisionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StableCollisionMesh->ClearAllMeshSections();

    const int32 TileCount = TerrainTileCountPerAxis();
    CollisionTileMeshes.Reserve(TileCount * TileCount);
    for (int32 TileY = 0; TileY < TileCount; ++TileY)
    {
        for (int32 TileX = 0; TileX < TileCount; ++TileX)
        {
            UProceduralMeshComponent* CollisionTile =
                NewObject<UProceduralMeshComponent>(
                    this,
                    *FString::Printf(
                        TEXT("MarsSoilCollision_%02d_%02d"),
                        TileX,
                        TileY
                    )
                );
            AddInstanceComponent(CollisionTile);
            CollisionTile->SetupAttachment(TerrainMesh);
            CollisionTile->SetMobility(EComponentMobility::Movable);
            CollisionTile->SetCollisionEnabled(
                ECollisionEnabled::QueryAndPhysics
            );
            CollisionTile->SetCollisionObjectType(ECC_WorldStatic);
            CollisionTile->SetCollisionResponseToAllChannels(ECR_Block);
            CollisionTile->bUseComplexAsSimpleCollision = true;
            CollisionTile->bUseAsyncCooking = true;
            CollisionTile->SetCastShadow(false);
            CollisionTile->SetVisibility(false, true);
            CollisionTile->SetHiddenInGame(true);
            CollisionTile->RegisterComponent();

            TArray<FVector> TileVertices;
            TArray<int32> TileTriangles;
            TArray<FVector> TileNormals;
            TArray<FVector2D> TileUVs;
            TArray<FLinearColor> TileColors;
            TArray<FProcMeshTangent> TileTangents;
            BuildTerrainTileData(
                TileX,
                TileY,
                TileVertices,
                TileTriangles,
                TileNormals,
                TileUVs,
                TileColors,
                TileTangents
            );
            CollisionTile->CreateMeshSection_LinearColor(
                0,
                TileVertices,
                TileTriangles,
                TileNormals,
                TileUVs,
                TileColors,
                TileTangents,
                true,
                false
            );
            CollisionTileMeshes.Add(CollisionTile);
        }
    }
    PendingCollisionTiles.Reset();
    CollisionUpdateAccumulator = 0.0f;
}

void ADiggableTerrain::UpdatePendingCollisionTiles(
    const float DeltaTime
)
{
    if (
        PendingCollisionTiles.IsEmpty()
        || CollisionTileMeshes.IsEmpty()
    )
    {
        return;
    }
    CollisionUpdateAccumulator += DeltaTime;
    if (CollisionUpdateAccumulator < CollisionUpdateIntervalSeconds)
    {
        return;
    }
    CollisionUpdateAccumulator = 0.0f;

    const int32 TileCount = TerrainTileCountPerAxis();
    const FVector2D Minimum = GridMinimum();
    const AActor* Machine =
        ExcavatorMesh.IsValid() ? ExcavatorMesh->GetOwner() : nullptr;
    const FVector MachineLocation =
        Machine ? Machine->GetActorLocation() : FVector::ZeroVector;

    int32 SelectedTileIndex = INDEX_NONE;
    for (const int32 TileIndex : PendingCollisionTiles)
    {
        if (!CollisionTileMeshes.IsValidIndex(TileIndex))
        {
            SelectedTileIndex = TileIndex;
            break;
        }
        const int32 TileX = TileIndex % TileCount;
        const int32 TileY = TileIndex / TileCount;
        const FVector LocalCenter(
            Minimum.X
                + (TileX * TileCellCount + TileCellCount * 0.5f)
                    * CellSizeCentimeters,
            Minimum.Y
                + (TileY * TileCellCount + TileCellCount * 0.5f)
                    * CellSizeCentimeters,
            0.0f
        );
        const FVector WorldCenter =
            GetActorTransform().TransformPosition(LocalCenter);
        if (
            Machine
            && FVector::DistSquared2D(WorldCenter, MachineLocation)
                < FMath::Square(CollisionSafetyRadiusCentimeters)
        )
        {
            continue;
        }
        SelectedTileIndex = TileIndex;
        break;
    }

    if (SelectedTileIndex == INDEX_NONE)
    {
        return;
    }
    if (!CollisionTileMeshes.IsValidIndex(SelectedTileIndex))
    {
        PendingCollisionTiles.Remove(SelectedTileIndex);
        return;
    }
    {
        const int32 TileIndex = SelectedTileIndex;
        const int32 TileX = TileIndex % TileCount;
        const int32 TileY = TileIndex / TileCount;
        TArray<FVector> TileVertices;
        TArray<int32> TileTriangles;
        TArray<FVector> TileNormals;
        TArray<FVector2D> TileUVs;
        TArray<FLinearColor> TileColors;
        TArray<FProcMeshTangent> TileTangents;
        BuildTerrainTileData(
            TileX,
            TileY,
            TileVertices,
            TileTriangles,
            TileNormals,
            TileUVs,
            TileColors,
            TileTangents
        );
        CollisionTileMeshes[TileIndex]->UpdateMeshSection_LinearColor(
            0,
            TileVertices,
            TileNormals,
            TileUVs,
            TileColors,
            TileTangents,
            false
        );
        PendingCollisionTiles.Remove(TileIndex);
    }
}

void ADiggableTerrain::ResolveExcavatorMesh()
{
    if (ExcavatorMesh.IsValid())
    {
        return;
    }

    for (TActorIterator<AActor> ActorIterator(GetWorld());
         ActorIterator;
         ++ActorIterator)
    {
        TArray<USkeletalMeshComponent*> MeshComponents;
        ActorIterator->GetComponents(MeshComponents);
        for (USkeletalMeshComponent* Mesh : MeshComponents)
        {
            if (
                Mesh
                && Mesh->GetBoneIndex(BucketBoneName) != INDEX_NONE
            )
            {
                ExcavatorMesh = Mesh;
                ExcavatorPawn = Cast<APawn>(ActorIterator.operator->());
                ExcavatorAdapter =
                    ActorIterator->FindComponentByClass<
                        UExcavatorVendorAdapterComponent
                    >();
                ExcavatorROSBridge =
                    ActorIterator->FindComponentByClass<
                        UExcavatorROSBridgeComponent
                    >();
                if (ExcavatorAdapter.IsValid())
                {
                    ObservedResetGeneration =
                        ExcavatorAdapter->GetResetGeneration();
                }
                if (ExcavatorROSBridge.IsValid())
                {
                    ObservedOperatorToggleGeneration =
                        ExcavatorROSBridge->
                            GetOperatorToggleGeneration();
                }
                // The former sphere load remains hidden so older placed actor
                // data cannot make it reappear at the hinge.
                BucketLoadMesh->SetVisibility(false, true);
                BucketLoadMesh->SetHiddenInGame(true);
                BucketSoilMesh->AttachToComponent(
                    Mesh,
                    FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                    BucketBoneName
                );
                BucketSoilMesh->SetRelativeLocation(
                    BucketSoilRelativeLocation
                );
                BucketSoilMesh->SetRelativeRotation(
                    BucketSoilRelativeRotation
                );
                UpdateBucketLoadVisual();
                bHasPreviousBucketTip = false;
                UE_LOG(
                    LogDiggableTerrain,
                    Log,
                    TEXT("Connected soil interaction to %s bone %s"),
                    *ActorIterator->GetName(),
                    *BucketBoneName.ToString()
                );
                InitializeGranularSoil();
                return;
            }
        }
    }
}

void ADiggableTerrain::ProcessBucketInteraction()
{
    if (!ExcavatorMesh.IsValid())
    {
        return;
    }

    const FTransform BucketTransform =
        ExcavatorMesh->GetSocketTransform(
            BucketBoneName,
            RTS_World
        );
    const FTransform BucketCavityTransform =
        BucketSoilMesh
        ? BucketSoilMesh->GetComponentTransform()
        : BucketTransform;

    // The Marketplace bucket's End-bone axes do not point toward its teeth.
    // BucketSoilMesh is calibrated to the visible bowl, with local +X pointing
    // through the open mouth. Derive every digging probe from that same frame
    // so the terrain contact, carried load, and dump stream agree.
    const float HalfToothWidth =
        BucketInteriorWidthCentimeters * 0.47f;
    const FVector CuttingLipLocal(
        BucketInteriorLengthCentimeters * 0.5f + 9.0f,
        0.0f,
        -BucketInteriorDepthCentimeters * 0.32f
    );
    const float InboardProbeDistance =
        FMath::Min(BucketProbeSpreadCentimeters, 12.0f);
    const FVector ProbeOffsets[] = {
        CuttingLipLocal,
        CuttingLipLocal + FVector(0.0f, -HalfToothWidth, 0.0f),
        CuttingLipLocal
            + FVector(0.0f, -HalfToothWidth * 0.5f, 0.0f),
        CuttingLipLocal
            + FVector(0.0f, HalfToothWidth * 0.5f, 0.0f),
        CuttingLipLocal + FVector(0.0f, HalfToothWidth, 0.0f),
        CuttingLipLocal
            + FVector(-InboardProbeDistance, -HalfToothWidth, 0.0f),
        CuttingLipLocal
            + FVector(
                -InboardProbeDistance,
                -HalfToothWidth * 0.5f,
                0.0f
            ),
        CuttingLipLocal
            + FVector(-InboardProbeDistance, 0.0f, 0.0f),
        CuttingLipLocal
            + FVector(
                -InboardProbeDistance,
                HalfToothWidth * 0.5f,
                0.0f
            ),
        CuttingLipLocal
            + FVector(-InboardProbeDistance, HalfToothWidth, 0.0f)
    };

    const FVector BucketTipWorld =
        BucketCavityTransform.TransformPosition(ProbeOffsets[0]);
    const FVector LocalBucketTip =
        GetActorTransform().InverseTransformPosition(BucketTipWorld);
    float BestSurfaceGap = TNumericLimits<float>::Max();
    for (const FVector& ProbeOffset : ProbeOffsets)
    {
        const FVector CandidateWorld =
            BucketCavityTransform.TransformPosition(ProbeOffset);
        const FVector CandidateLocal =
            GetActorTransform().InverseTransformPosition(CandidateWorld);
        const FVector2D Candidate2D(CandidateLocal);
        if (!IsInsideTerrain(Candidate2D))
        {
            continue;
        }

        const float SurfaceGap =
            CandidateLocal.Z - SampleHeight(Candidate2D);
        if (SurfaceGap < BestSurfaceGap)
        {
            BestSurfaceGap = SurfaceGap;
        }

        if (
            FParse::Param(
                FCommandLine::Get(),
                TEXT("DebugBucketContact")
            )
        )
        {
            DrawDebugSphere(
                GetWorld(),
                CandidateWorld,
                3.5f,
                8,
                ProbeOffset.Equals(CuttingLipLocal)
                    ? FColor::Yellow
                    : FColor::Green,
                false,
                InteractionIntervalSeconds * 1.5f,
                0,
                1.2f
            );
        }
    }

    FVector2D LocalMotion = FVector2D::ZeroVector;
    if (bHasPreviousBucketTip)
    {
        const FVector LastLocal =
            GetActorTransform().InverseTransformPosition(
                LastBucketTipWorld
            );
        LocalMotion = FVector2D(LocalBucketTip - LastLocal);
    }
    LastBucketTipWorld = BucketTipWorld;
    bHasPreviousBucketTip = true;

    const FVector2D Tip2D(LocalBucketTip);
    if (!IsInsideTerrain(Tip2D))
    {
        bWasDigging = false;
        return;
    }

    const float SurfaceHeight = SampleHeight(Tip2D);
    if (!bLoggedInitialBucketTip)
    {
        bLoggedInitialBucketTip = true;
        UE_LOG(
            LogDiggableTerrain,
            Log,
            TEXT(
                "Initial bucket tip local=(%.1f, %.1f, %.1f), "
                "soil surface=%.1f"
            ),
            LocalBucketTip.X,
            LocalBucketTip.Y,
            LocalBucketTip.Z,
            SurfaceHeight
        );
    }
    const bool bNearSurface =
        BestSurfaceGap <= ContactToleranceCentimeters;
    const bool bMoved =
        LocalMotion.SizeSquared()
        >= FMath::Square(MinimumTipMotionCentimeters);

    if (ExcavatorAdapter.IsValid())
    {
        const bool bPenetrationBlocked =
            LocalBucketTip.Z
            < SurfaceHeight - MaximumBucketPenetrationCentimeters;
        ExcavatorAdapter->SetSoilPenetrationBlocked(
            bPenetrationBlocked
        );

        const float DumpCommand = FMath::Clamp(
            ExcavatorAdapter->GetBucketInput(),
            0.0f,
            1.0f
        );
        const bool bLoadRaised =
            LocalBucketTip.Z > SurfaceHeight + 45.0f;

        // Cavity local +X points through the cutting edge after calibration.
        const FVector WorldForward =
            BucketCavityTransform.TransformVectorNoScale(
                FVector::ForwardVector
            ).GetSafeNormal();
        const float DownwardLipAmount =
            FMath::Clamp(-WorldForward.Z, 0.0f, 1.0f);
        const float TiltFactor = FMath::Clamp(
            (
                DownwardLipAmount - DumpTiltThreshold
            )
            / FMath::Max(1.0f - DumpTiltThreshold, 0.01f),
            0.0f,
            1.0f
        );
        const float EffectiveDumpAmount =
            DumpCommand * FMath::Max(TiltFactor, 0.25f);
        if (
            EffectiveDumpAmount > 0.02f
            && bLoadRaised
            && CarriedSoilVolumeCubicCentimeters
                > KINDA_SMALL_NUMBER
        )
        {
            const FVector LocalForward3D =
                GetActorTransform().InverseTransformVectorNoScale(
                    WorldForward
                );
            // Emit at the open cutting lip, not at the animated linkage pin.
            const FVector PourLipWorld =
                BucketCavityTransform.TransformPosition(
                    FVector(
                        BucketInteriorLengthCentimeters * 0.5f + 9.0f,
                        0.0f,
                        -BucketInteriorDepthCentimeters * 0.30f
                    )
                );
            const FVector LocalPourLip =
                GetActorTransform().InverseTransformPosition(
                    PourLipWorld
                );
            DumpBucketLoad(
                LocalPourLip,
                FVector2D(LocalForward3D).GetSafeNormal(),
                DumpRateCubicMetersPerSecond
                    * 1000000.0f
                    * InteractionIntervalSeconds
                    * EffectiveDumpAmount
            );
        }
        else
        {
            bWasDumping = false;
        }
    }

    bool bDugThisStep = false;
    if (bNearSurface && bMoved)
    {
        const FVector WorldAcross =
            BucketCavityTransform.TransformVectorNoScale(
                FVector::RightVector
            ).GetSafeNormal();
        const FVector LocalAcross3D =
            GetActorTransform().InverseTransformVectorNoScale(
                WorldAcross
            );
        FVector ContactTip = LocalBucketTip;
        ContactTip.Z = FMath::Min(
            ContactTip.Z,
            SurfaceHeight + BestSurfaceGap
        );
        bDugThisStep = DigAt(
            ContactTip,
            LocalMotion,
            FVector2D(LocalAcross3D).GetSafeNormal()
        );
    }

    bWasDigging = bDugThisStep || (bWasDigging && bNearSurface);
    if (CarriedSoilVolumeCubicCentimeters > KINDA_SMALL_NUMBER)
    {
        // Rebuild the small surface as the bucket rotates so the loose soil's
        // top remains approximately level with gravity instead of looking
        // glued into the bucket.
        UpdateBucketLoadVisual();
    }
}

bool ADiggableTerrain::DigAt(
    const FVector& LocalBucketTip,
    const FVector2D& LocalMotion,
    const FVector2D& LocalBucketAcross
)
{
    const FVector2D Tip2D(LocalBucketTip);
    const FVector2D Minimum = GridMinimum();
    const int32 CenterX = FMath::RoundToInt(
        (Tip2D.X - Minimum.X) / CellSizeCentimeters
    );
    const int32 CenterY = FMath::RoundToInt(
        (Tip2D.Y - Minimum.Y) / CellSizeCentimeters
    );
    const float HalfCutWidth =
        BucketInteriorWidthCentimeters * 0.50f;
    const float MotionLength = LocalMotion.Size();
    FVector2D CutForward = LocalMotion.GetSafeNormal();
    FVector2D CutAcross = LocalBucketAcross.GetSafeNormal();
    if (CutAcross.IsNearlyZero())
    {
        CutAcross = FVector2D(-CutForward.Y, CutForward.X);
    }
    if (CutForward.IsNearlyZero())
    {
        CutForward = FVector2D(CutAcross.Y, -CutAcross.X);
    }
    // Project out any camera/skeleton skew so the footprint remains the
    // actual bucket width rather than an oversized circular brush.
    CutAcross = (
        CutAcross - CutForward * FVector2D::DotProduct(
            CutAcross,
            CutForward
        )
    ).GetSafeNormal();
    if (CutAcross.IsNearlyZero())
    {
        CutAcross = FVector2D(-CutForward.Y, CutForward.X);
    }
    const float ToothPadding = 22.0f;
    const float SearchRadius =
        HalfCutWidth + MotionLength + ToothPadding;
    const int32 RadiusCells = FMath::CeilToInt(
        SearchRadius / CellSizeCentimeters
    );
    const float MotionScale = FMath::Clamp(
        LocalMotion.Size() / 8.0f,
        0.35f,
        1.0f
    );
    const float TipSurfaceHeight = SampleHeight(Tip2D);
    const float ContactPressure = FMath::Clamp(
        (
            TipSurfaceHeight
            + ContactToleranceCentimeters
            - LocalBucketTip.Z
        )
        / FMath::Max(ContactToleranceCentimeters, 1.0f),
        0.0f,
        1.0f
    );

    float RemovedHeightSum = 0.0f;
    bool bChanged = false;
    for (int32 Y = CenterY - RadiusCells;
         Y <= CenterY + RadiusCells;
         ++Y)
    {
        if (Y < 0 || Y > GridResolution)
        {
            continue;
        }
        for (int32 X = CenterX - RadiusCells;
             X <= CenterX + RadiusCells;
             ++X)
        {
            if (X < 0 || X > GridResolution)
            {
                continue;
            }

            const int32 Index = VertexIndex(X, Y);
            const FVector2D Position(
                Minimum.X + X * CellSizeCentimeters,
                Minimum.Y + Y * CellSizeCentimeters
            );
            const FVector2D Offset = Position - Tip2D;
            const float Across =
                FVector2D::DotProduct(Offset, CutAcross);
            const float Along =
                FVector2D::DotProduct(Offset, CutForward);
            const float RearExtent = MotionLength + ToothPadding;
            if (
                FMath::Abs(Across) >= HalfCutWidth
                || Along < -RearExtent
                || Along > ToothPadding
            )
            {
                continue;
            }

            const float AcrossWeight = FMath::Pow(
                1.0f - FMath::Abs(Across) / HalfCutWidth,
                0.45f
            );
            const float AlongCenter =
                (ToothPadding - RearExtent) * 0.5f;
            const float AlongHalfSpan =
                FMath::Max(
                    (ToothPadding + RearExtent) * 0.5f,
                    1.0f
                );
            const float AlongWeight = FMath::Pow(
                FMath::Clamp(
                    1.0f
                        - FMath::Abs(Along - AlongCenter)
                            / AlongHalfSpan,
                    0.0f,
                    1.0f
                ),
                0.28f
            );
            const float Falloff = AcrossWeight * AlongWeight;
            const float GeometricHeight =
                LocalBucketTip.Z
                - 14.0f
                + FMath::Abs(Across) * 0.035f;
            const float CompressedHeight =
                CurrentHeights[Index]
                - MaximumCutPerStepCentimeters
                * ContactPressure
                * 0.7f
                * Falloff;
            const float DesiredHeight =
                FMath::Min(GeometricHeight, CompressedHeight);
            const float MinimumHeight =
                InitialHeights[Index] - MaximumDigDepthCentimeters;
            const float CutNeeded =
                CurrentHeights[Index]
                - FMath::Max(DesiredHeight, MinimumHeight);
            if (CutNeeded <= 0.0f)
            {
                continue;
            }

            const float Cut = FMath::Min(
                CutNeeded,
                MaximumCutPerStepCentimeters
                * Falloff
                * MotionScale
            );
            CurrentHeights[Index] -= Cut;
            MarkTerrainVertexDirty(X, Y);
            RemovedHeightSum += Cut;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        LastDigLocation = Tip2D;
        const float RemovedVolumeCubicCentimeters =
            RemovedHeightSum
            * FMath::Square(CellSizeCentimeters);
        const float CapacityCubicCentimeters =
            FMath::Max(
                BucketCapacityCubicMeters * 1000000.0f,
                1.0f
            );
        const float AvailableCapacity =
            FMath::Max(
                CapacityCubicCentimeters
                    - CarriedSoilVolumeCubicCentimeters,
                0.0f
            );

        float CaptureEfficiency = PassiveCaptureEfficiency;
        if (ExcavatorAdapter.IsValid())
        {
            const float CurlAmount = FMath::Clamp(
                -ExcavatorAdapter->GetBucketInput(),
                0.0f,
                1.0f
            );
            CaptureEfficiency = FMath::Lerp(
                PassiveCaptureEfficiency,
                CurlCaptureEfficiency,
                CurlAmount
            );
            if (ExcavatorAdapter->GetBucketInput() > 0.05f)
            {
                CaptureEfficiency = 0.0f;
            }
        }
        CaptureEfficiency = FMath::Clamp(
            CaptureEfficiency,
            0.0f,
            1.0f
        );

        const float UncappedCapturedVolume =
            RemovedVolumeCubicCentimeters * CaptureEfficiency;
        const float ContactRateScale = FMath::Lerp(
            0.35f,
            1.0f,
            ContactPressure
        );
        const float MaximumCapturedThisStep =
            CapacityCubicCentimeters
            * MaximumBucketFillFractionPerSecond
            * InteractionIntervalSeconds
            * ContactRateScale;
        const float CapturedVolume = FMath::Min3(
            UncappedCapturedVolume,
            MaximumCapturedThisStep,
            AvailableCapacity
        );
        CarriedSoilVolumeCubicCentimeters = FMath::Min(
            CarriedSoilVolumeCubicCentimeters + CapturedVolume,
            CapacityCubicCentimeters
        );

        // Soil that misses a curling bucket, or is cut after the bucket is
        // full, forms a short windrow instead of disappearing.
        const float DisplacedVolume =
            RemovedVolumeCubicCentimeters - CapturedVolume;
        if (DisplacedVolume > KINDA_SMALL_NUMBER)
        {
            FVector2D PushDirection = LocalMotion.GetSafeNormal();
            if (PushDirection.IsNearlyZero())
            {
                PushDirection = FVector2D(1.0f, 0.0f);
            }
            DepositSpoil(
                Tip2D
                    + PushDirection
                        * BucketInteriorLengthCentimeters
                        * 0.55f,
                DisplacedVolume
            );
        }

        UpdateBucketLoadVisual();
        UpdateTerrainMesh();
        if (!bLoggedFirstDig)
        {
            bLoggedFirstDig = true;
            UE_LOG(
                LogDiggableTerrain,
                Log,
                TEXT("Bucket engaged and deformed Mars soil")
            );
        }
        if (
            !bLoggedFullBucket
            && CarriedSoilVolumeCubicCentimeters
                >= CapacityCubicCentimeters * 0.95f
        )
        {
            bLoggedFullBucket = true;
            UE_LOG(LogDiggableTerrain, Log, TEXT("Bucket load is full"));
        }
    }
    return bChanged;
}

void ADiggableTerrain::DepositSpoil(
    const FVector2D& Center,
    const float VolumeCubicCentimeters
)
{
    if (
        VolumeCubicCentimeters <= KINDA_SMALL_NUMBER
        || !IsInsideTerrain(Center)
    )
    {
        return;
    }

    const float HeightSum =
        VolumeCubicCentimeters
        / FMath::Max(FMath::Square(CellSizeCentimeters), 1.0f);
    const float PileRadius = FMath::Max(
        DigRadiusCentimeters * 1.35f,
        FMath::Pow(VolumeCubicCentimeters, 1.0f / 3.0f) * 1.6f
    );
    const FVector2D Minimum = GridMinimum();
    const int32 CenterX = FMath::RoundToInt(
        (Center.X - Minimum.X) / CellSizeCentimeters
    );
    const int32 CenterY = FMath::RoundToInt(
        (Center.Y - Minimum.Y) / CellSizeCentimeters
    );
    const int32 RadiusCells = FMath::CeilToInt(
        PileRadius / CellSizeCentimeters
    );

    struct FDepositVertex
    {
        int32 Index = INDEX_NONE;
        float Weight = 0.0f;
    };
    TArray<FDepositVertex> DepositVertices;
    float TotalWeight = 0.0f;

    for (int32 Y = CenterY - RadiusCells;
         Y <= CenterY + RadiusCells;
         ++Y)
    {
        if (Y < 0 || Y > GridResolution)
        {
            continue;
        }
        for (int32 X = CenterX - RadiusCells;
             X <= CenterX + RadiusCells;
             ++X)
        {
            if (X < 0 || X > GridResolution)
            {
                continue;
            }

            const FVector2D Position(
                Minimum.X + X * CellSizeCentimeters,
                Minimum.Y + Y * CellSizeCentimeters
            );
            const float Distance = FVector2D::Distance(Position, Center);
            if (Distance >= PileRadius)
            {
                continue;
            }

            const float Weight = FMath::Square(
                1.0f - Distance / PileRadius
            );
            DepositVertices.Add({VertexIndex(X, Y), Weight});
            TotalWeight += Weight;
        }
    }

    if (TotalWeight <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    for (const FDepositVertex& Deposit : DepositVertices)
    {
        const float AddedHeight =
            HeightSum * Deposit.Weight / TotalWeight;
        CurrentHeights[Deposit.Index] += AddedHeight;
        const int32 X = Deposit.Index % (GridResolution + 1);
        const int32 Y = Deposit.Index / (GridResolution + 1);
        MarkTerrainVertexDirty(X, Y);
    }
    RelaxSpoilPile(Center, PileRadius + CellSizeCentimeters * 3.0f);
}

void ADiggableTerrain::RelaxSpoilPile(
    const FVector2D& Center,
    const float Radius
)
{
    const FVector2D Minimum = GridMinimum();
    const int32 CenterX = FMath::RoundToInt(
        (Center.X - Minimum.X) / CellSizeCentimeters
    );
    const int32 CenterY = FMath::RoundToInt(
        (Center.Y - Minimum.Y) / CellSizeCentimeters
    );
    const int32 RadiusCells =
        FMath::CeilToInt(Radius / CellSizeCentimeters);
    const float MaximumStep =
        FMath::Tan(FMath::DegreesToRadians(35.0f))
        * CellSizeCentimeters;

    for (int32 Iteration = 0; Iteration < 3; ++Iteration)
    {
        for (int32 Y = FMath::Max(CenterY - RadiusCells, 0);
             Y <= FMath::Min(CenterY + RadiusCells, GridResolution);
             ++Y)
        {
            for (int32 X = FMath::Max(CenterX - RadiusCells, 0);
                 X <= FMath::Min(CenterX + RadiusCells, GridResolution);
                 ++X)
            {
                const int32 NeighborOffsets[][2] = {
                    {1, 0},
                    {0, 1}
                };
                for (const auto& Offset : NeighborOffsets)
                {
                    const int32 NeighborX = X + Offset[0];
                    const int32 NeighborY = Y + Offset[1];
                    if (
                        NeighborX > GridResolution
                        || NeighborY > GridResolution
                    )
                    {
                        continue;
                    }
                    const int32 Index = VertexIndex(X, Y);
                    const int32 NeighborIndex =
                        VertexIndex(NeighborX, NeighborY);
                    const float Difference =
                        CurrentHeights[Index]
                        - CurrentHeights[NeighborIndex];
                    const float Excess =
                        FMath::Abs(Difference) - MaximumStep;
                    if (Excess <= KINDA_SMALL_NUMBER)
                    {
                        continue;
                    }
                    const float Transfer =
                        FMath::Sign(Difference) * Excess * 0.5f;
                    CurrentHeights[Index] -= Transfer;
                    CurrentHeights[NeighborIndex] += Transfer;
                    MarkTerrainVertexDirty(X, Y);
                    MarkTerrainVertexDirty(NeighborX, NeighborY);
                }
            }
        }
    }
}

float ADiggableTerrain::SampleHeight(
    const FVector2D& LocalPosition
) const
{
    if (CurrentHeights.IsEmpty())
    {
        return 0.0f;
    }

    const FVector2D Minimum = GridMinimum();
    const float GridX =
        (LocalPosition.X - Minimum.X) / CellSizeCentimeters;
    const float GridY =
        (LocalPosition.Y - Minimum.Y) / CellSizeCentimeters;
    const int32 X0 = FMath::Clamp(
        FMath::FloorToInt(GridX),
        0,
        GridResolution
    );
    const int32 Y0 = FMath::Clamp(
        FMath::FloorToInt(GridY),
        0,
        GridResolution
    );
    const int32 X1 = FMath::Min(X0 + 1, GridResolution);
    const int32 Y1 = FMath::Min(Y0 + 1, GridResolution);
    const float AlphaX = FMath::Clamp(GridX - X0, 0.0f, 1.0f);
    const float AlphaY = FMath::Clamp(GridY - Y0, 0.0f, 1.0f);

    const float Bottom = FMath::Lerp(
        CurrentHeights[VertexIndex(X0, Y0)],
        CurrentHeights[VertexIndex(X1, Y0)],
        AlphaX
    );
    const float Top = FMath::Lerp(
        CurrentHeights[VertexIndex(X0, Y1)],
        CurrentHeights[VertexIndex(X1, Y1)],
        AlphaX
    );
    return FMath::Lerp(Bottom, Top, AlphaY);
}

int32 ADiggableTerrain::VertexIndex(
    const int32 X,
    const int32 Y
) const
{
    return Y * (GridResolution + 1) + X;
}

FVector2D ADiggableTerrain::GridMinimum() const
{
    const float HalfExtent =
        GridResolution * CellSizeCentimeters * 0.5f;
    return FVector2D(-HalfExtent, -HalfExtent);
}

bool ADiggableTerrain::IsInsideTerrain(
    const FVector2D& LocalPosition
) const
{
    const float HalfExtent =
        GridResolution * CellSizeCentimeters * 0.5f
        - DigRadiusCentimeters;
    return
        FMath::Abs(LocalPosition.X) <= HalfExtent
        && FMath::Abs(LocalPosition.Y) <= HalfExtent;
}

void ADiggableTerrain::ResetTerrain()
{
    if (InitialHeights.Num() != CurrentHeights.Num())
    {
        GenerateTerrain();
        return;
    }

    CurrentHeights = InitialHeights;
    CarriedSoilVolumeCubicCentimeters = 0.0f;
    bWasDigging = false;
    bWasDumping = false;
    bHasPreviousBucketTip = false;
    bLoggedFirstDig = false;
    bLoggedFullBucket = false;
    if (ExcavatorAdapter.IsValid())
    {
        ExcavatorAdapter->SetSoilPenetrationBlocked(false);
    }
    UpdateBucketLoadVisual();
    ResetGranularSoil();
    DumpStreamParticles.Reset();
    UpdateDumpStreamInstances();
    MarkAllTerrainTilesDirty();
    UpdateTerrainMesh();
    InitializeCollisionTiles();
    UE_LOG(LogDiggableTerrain, Log, TEXT("Mars soil reset"));
}

void ADiggableTerrain::UpdateResetInput()
{
    APlayerController* PlayerController =
        GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PlayerController)
    {
        return;
    }

    if (bOperatorOnFoot)
    {
        bResetButtonWasDown = false;
        return;
    }

    const bool bResetButtonDown =
        PlayerController->IsInputKeyDown(EKeys::Gamepad_Special_Right)
        || PlayerController->IsInputKeyDown(EKeys::R);
    if (bResetButtonDown && !bResetButtonWasDown)
    {
        ResetTerrain();
    }
    bResetButtonWasDown = bResetButtonDown;
}

void ADiggableTerrain::UpdateOperatorModeInput()
{
    APlayerController* PlayerController =
        GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

    bool bRemoteToggleRequested = false;
    if (ExcavatorROSBridge.IsValid())
    {
        const uint32 CurrentGeneration =
            ExcavatorROSBridge->GetOperatorToggleGeneration();
        bRemoteToggleRequested =
            CurrentGeneration != ObservedOperatorToggleGeneration;
        ObservedOperatorToggleGeneration = CurrentGeneration;
    }

    if (!PlayerController && !bRemoteToggleRequested)
    {
        bOperatorToggleWasDown = false;
        return;
    }

    const bool bToggleButtonDown =
        PlayerController
        && (
            PlayerController->IsInputKeyDown(
                EKeys::Gamepad_FaceButton_Top
            )
            || PlayerController->IsInputKeyDown(EKeys::Y)
            || PlayerController->IsInputKeyDown(EKeys::E)
        );

    if (
        bRemoteToggleRequested
        || (bToggleButtonDown && !bOperatorToggleWasDown)
    )
    {
        if (bOperatorOnFoot)
        {
            TryEnterExcavator();
        }
        else
        {
            ExitExcavator();
        }
    }
    bOperatorToggleWasDown = bToggleButtonDown;
}

void ADiggableTerrain::ExitExcavator()
{
    if (bOperatorOnFoot || !GetWorld())
    {
        return;
    }

    ResolveExcavatorMesh();
    APawn* MachinePawn = ExcavatorPawn.Get();
    AActor* Machine =
        ExcavatorMesh.IsValid() ? ExcavatorMesh->GetOwner() : nullptr;
    APlayerController* PlayerController =
        GetWorld()->GetFirstPlayerController();
    if (!MachinePawn || !Machine || !PlayerController)
    {
        UE_LOG(
            LogDiggableTerrain,
            Warning,
            TEXT("Cannot exit excavator: machine pawn is not ready")
        );
        return;
    }

    FTransform ExitTransform;
    if (!FindOperatorExitTransform(ExitTransform))
    {
        return;
    }

    AExcavatorOperatorCharacter* Operator = OperatorCharacter.Get();
    if (!Operator)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::
                AdjustIfPossibleButAlwaysSpawn;
        Operator =
            GetWorld()->SpawnActor<AExcavatorOperatorCharacter>(
                AExcavatorOperatorCharacter::StaticClass(),
                ExitTransform,
                SpawnParameters
            );
        OperatorCharacter = Operator;
    }
    else
    {
        Operator->SetActorTransform(
            ExitTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }

    if (!Operator)
    {
        UE_LOG(
            LogDiggableTerrain,
            Error,
            TEXT("Could not spawn the excavator operator")
        );
        return;
    }

    SavedExcavatorControlRotation =
        PlayerController->GetControlRotation();

    Operator->SetActorHiddenInGame(false);
    Operator->SetActorEnableCollision(true);
    Operator->SetActorTickEnabled(true);
    if (Operator->GetCharacterMovement())
    {
        Operator->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    bOperatorOnFoot = true;
    if (ExcavatorAdapter.IsValid())
    {
        ExcavatorAdapter->SetOperatorOnFoot(true);
    }

    PlayerController->Possess(Operator);
    PlayerController->ConsoleCommand(
        TEXT("r.DepthOfFieldQuality 0"),
        false
    );

    const FVector ViewTowardMachine =
        Machine->GetActorLocation() - Operator->GetActorLocation();
    const float InitialOperatorViewYaw =
        ViewTowardMachine.IsNearlyZero()
            ? Machine->GetActorRotation().Yaw
            : ViewTowardMachine.Rotation().Yaw;
    PlayerController->SetControlRotation(
        FRotator(
            -12.0f,
            InitialOperatorViewYaw,
            0.0f
        )
    );
    PlayerController->SetViewTargetWithBlend(
        Operator,
        0.30f,
        EViewTargetBlendFunction::VTBlend_EaseInOut
    );
    if (ExcavatorROSBridge.IsValid())
    {
        ExcavatorROSBridge->SetOperatorOnFoot(true);
    }
    if (
        UExcavatorSensorRigComponent* SensorRig =
            Machine->FindComponentByClass<
                UExcavatorSensorRigComponent
            >()
    )
    {
        SensorRig->SetOperatorMode(true);
    }

    UE_LOG(
        LogDiggableTerrain,
        Log,
        TEXT("Operator exited %s at %s"),
        *Machine->GetName(),
        *Operator->GetActorLocation().ToCompactString()
    );
}

void ADiggableTerrain::TryEnterExcavator(
    const bool bIgnoreDistance
)
{
    AExcavatorOperatorCharacter* Operator = OperatorCharacter.Get();
    APawn* MachinePawn = ExcavatorPawn.Get();
    AActor* Machine =
        ExcavatorMesh.IsValid() ? ExcavatorMesh->GetOwner() : nullptr;
    APlayerController* PlayerController =
        GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (
        !bOperatorOnFoot
        || !Operator
        || !MachinePawn
        || !Machine
        || !PlayerController
    )
    {
        return;
    }

    const float DistanceToMachine =
        FVector::Dist2D(
            Operator->GetActorLocation(),
            Machine->GetActorLocation()
        );
    if (
        !bIgnoreDistance
        && DistanceToMachine > ExcavatorEnterDistanceCentimeters
    )
    {
        UE_LOG(
            LogDiggableTerrain,
            Log,
            TEXT(
                "Operator is %.0f cm from the excavator; "
                "move within %.0f cm to enter"
            ),
            DistanceToMachine,
            ExcavatorEnterDistanceCentimeters
        );
        return;
    }

    if (Operator->GetCharacterMovement())
    {
        Operator->GetCharacterMovement()->StopMovementImmediately();
        Operator->GetCharacterMovement()->DisableMovement();
    }
    Operator->SetActorEnableCollision(false);
    Operator->SetActorHiddenInGame(true);

    bOperatorOnFoot = false;
    PlayerController->Possess(MachinePawn);
    PlayerController->ConsoleCommand(
        TEXT("r.DepthOfFieldQuality 2"),
        false
    );
    PlayerController->SetControlRotation(SavedExcavatorControlRotation);
    PlayerController->SetViewTargetWithBlend(
        MachinePawn,
        0.30f,
        EViewTargetBlendFunction::VTBlend_EaseInOut
    );
    if (ExcavatorAdapter.IsValid())
    {
        ExcavatorAdapter->SetOperatorOnFoot(false);
    }
    if (ExcavatorROSBridge.IsValid())
    {
        ExcavatorROSBridge->SetOperatorOnFoot(false);
    }
    if (
        UExcavatorSensorRigComponent* SensorRig =
            Machine->FindComponentByClass<
                UExcavatorSensorRigComponent
            >()
    )
    {
        SensorRig->SetOperatorMode(false);
    }

    UE_LOG(
        LogDiggableTerrain,
        Log,
        TEXT("Operator entered %s"),
        *Machine->GetName()
    );
}

bool ADiggableTerrain::FindOperatorExitTransform(
    FTransform& OutTransform
) const
{
    if (!GetWorld() || !ExcavatorMesh.IsValid())
    {
        return false;
    }

    AActor* Machine = ExcavatorMesh->GetOwner();
    if (!Machine)
    {
        return false;
    }

    FVector BoundsOrigin = FVector::ZeroVector;
    FVector BoundsExtent = FVector::ZeroVector;
    Machine->GetActorBounds(
        true,
        BoundsOrigin,
        BoundsExtent,
        true
    );
    const float SideOffset = FMath::Clamp(
        BoundsExtent.Y + 85.0f,
        180.0f,
        320.0f
    );

    FCollisionQueryParams QueryParameters(
        SCENE_QUERY_STAT(ExcavatorOperatorExit),
        false,
        Machine
    );

    const FVector MachineLocation = Machine->GetActorLocation();
    const FVector MachineRight =
        Machine->GetActorTransform().GetUnitAxis(EAxis::Y);
    FVector SelectedLocation =
        MachineLocation + MachineRight * SideOffset;
    bool bFoundGround = false;

    for (const float Side : {1.0f, -1.0f})
    {
        const FVector Candidate =
            MachineLocation + MachineRight * SideOffset * Side;

        // Prefer the authoritative deformable height field directly. Runtime
        // collision tiles cook asynchronously, and a trace during the first
        // few frames can otherwise select the decorative floor underneath.
        const FVector CandidateLocal =
            GetActorTransform().InverseTransformPosition(Candidate);
        const FVector2D CandidateLocal2D(
            CandidateLocal.X,
            CandidateLocal.Y
        );
        if (IsInsideTerrain(CandidateLocal2D))
        {
            const FVector SurfaceWorld =
                GetActorTransform().TransformPosition(
                    FVector(
                        CandidateLocal.X,
                        CandidateLocal.Y,
                        SampleHeight(CandidateLocal2D)
                    )
                );
            SelectedLocation =
                SurfaceWorld + FVector::UpVector * 101.0f;
            bFoundGround = true;
            UE_LOG(
                LogDiggableTerrain,
                Log,
                TEXT(
                    "Operator exit grounded on deformable terrain "
                    "at Z=%.1f"
                ),
                SurfaceWorld.Z
            );
            break;
        }

        const FVector TraceStart =
            FVector(Candidate.X, Candidate.Y, BoundsOrigin.Z + 600.0f);
        const FVector TraceEnd =
            FVector(Candidate.X, Candidate.Y, BoundsOrigin.Z - 1200.0f);
        FHitResult GroundHit;
        if (
            GetWorld()->LineTraceSingleByChannel(
                GroundHit,
                TraceStart,
                TraceEnd,
                ECC_Visibility,
                QueryParameters
            )
        )
        {
            SelectedLocation =
                GroundHit.Location + FVector::UpVector * 101.0f;
            bFoundGround = true;
            UE_LOG(
                LogDiggableTerrain,
                Log,
                TEXT("Operator exit ground: %s / %s at Z=%.1f"),
                GroundHit.GetActor()
                    ? *GroundHit.GetActor()->GetName()
                    : TEXT("none"),
                GroundHit.GetComponent()
                    ? *GroundHit.GetComponent()->GetName()
                    : TEXT("none"),
                GroundHit.Location.Z
            );
            break;
        }
    }

    if (!bFoundGround)
    {
        SelectedLocation.Z = MachineLocation.Z + 101.0f;
    }

    OutTransform = FTransform(
        FRotator(0.0f, Machine->GetActorRotation().Yaw, 0.0f),
        SelectedLocation,
        FVector::OneVector
    );
    return true;
}

void ADiggableTerrain::UpdateRosReset()
{
    if (!ExcavatorAdapter.IsValid())
    {
        return;
    }

    const uint32 ResetGeneration =
        ExcavatorAdapter->GetResetGeneration();
    if (ResetGeneration != ObservedResetGeneration)
    {
        ObservedResetGeneration = ResetGeneration;
        if (bOperatorOnFoot)
        {
            // Reset is a complete return-to-start operation. Unlike the Y
            // enter action, it must recover the operator even if Manny has
            // walked farther than the normal five-meter entry radius.
            TryEnterExcavator(true);
        }
        ResetTerrain();
    }
}

void ADiggableTerrain::UpdateBucketLoadVisual()
{
    if (!BucketSoilMesh)
    {
        return;
    }

    const float CapacityCubicCentimeters =
        FMath::Max(BucketCapacityCubicMeters * 1000000.0f, 1.0f);
    const float FillRatio = FMath::Clamp(
        CarriedSoilVolumeCubicCentimeters
        / CapacityCubicCentimeters,
        0.0f,
        1.0f
    );
    const bool bHasLoad = FillRatio > 0.015f;
    BucketLoadMesh->SetVisibility(false, true);
    BucketLoadMesh->SetHiddenInGame(true);
    BucketSoilMesh->SetVisibility(bHasLoad, true);
    BucketSoilMesh->SetHiddenInGame(!bHasLoad);
    if (!bHasLoad)
    {
        BucketSoilMesh->ClearAllMeshSections();
        return;
    }

    BuildBucketLoadMesh(FillRatio);
}

void ADiggableTerrain::BuildBucketLoadMesh(const float FillRatio)
{
    constexpr int32 LengthSegments = 8;
    constexpr int32 WidthSegments = 6;
    const int32 RowSize = LengthSegments + 1;
    const int32 SurfaceVertexCount =
        (LengthSegments + 1) * (WidthSegments + 1);

    TArray<FVector> LoadVertices;
    TArray<int32> LoadTriangles;
    TArray<FVector> LoadNormals;
    TArray<FVector2D> LoadUVs;
    TArray<FLinearColor> LoadColors;
    TArray<FProcMeshTangent> LoadTangents;
    LoadVertices.Reserve(SurfaceVertexCount * 2);
    LoadUVs.Reserve(SurfaceVertexCount * 2);
    LoadColors.Reserve(SurfaceVertexCount * 2);

    const float Growth = FMath::Pow(FillRatio, 0.55f);
    const float CoveredLength =
        BucketInteriorLengthCentimeters
        * FMath::Lerp(0.20f, 1.0f, Growth);
    const float CoveredHalfWidth =
        BucketInteriorWidthCentimeters
        * 0.5f
        * FMath::Lerp(0.32f, 1.0f, Growth);
    const float CenterX =
        -BucketInteriorLengthCentimeters * 0.5f
        + CoveredLength * 0.5f;
    const float HalfLength = CoveredLength * 0.5f;
    const float Depth = BucketInteriorDepthCentimeters;
    const float VerticalFill = FMath::Pow(FillRatio, 0.85f);

    FVector GravityUpLocal = BucketSoilMesh
        ->GetComponentTransform()
        .InverseTransformVectorNoScale(FVector::UpVector)
        .GetSafeNormal();
    if (FMath::Abs(GravityUpLocal.Z) < 0.28f)
    {
        GravityUpLocal = FVector::UpVector;
    }

    const auto BottomHeight = [Depth](
        const float FullBucketU,
        const float Across
    )
    {
        const float Longitudinal =
            2.0f * FullBucketU - 1.0f;
        return
            -Depth * 0.42f
            + Depth * 0.28f * FMath::Square(Longitudinal)
            - Depth * 0.18f * FullBucketU
            + Depth * 0.07f * FMath::Square(Across);
    };
    const auto MaximumSurfaceHeight = [Depth](
        const float FullBucketU
    )
    {
        return
            Depth * 0.20f
            - Depth * 0.16f * FullBucketU;
    };

    const float CenterFullU = FMath::Clamp(
        (
            CenterX
            + BucketInteriorLengthCentimeters * 0.5f
        )
        / BucketInteriorLengthCentimeters,
        0.0f,
        1.0f
    );
    const float CenterBottom = BottomHeight(CenterFullU, 0.0f);
    const float CenterTopLimit =
        MaximumSurfaceHeight(CenterFullU);
    const float PlaneCenterZ = FMath::Lerp(
        CenterBottom + 2.0f,
        CenterTopLimit,
        VerticalFill
    );

    for (int32 Surface = 0; Surface < 2; ++Surface)
    {
        const bool bTopSurface = Surface == 0;
        for (int32 Y = 0; Y <= WidthSegments; ++Y)
        {
            const float Across =
                -1.0f
                + 2.0f * static_cast<float>(Y) / WidthSegments;
            for (int32 X = 0; X <= LengthSegments; ++X)
            {
                const float Along =
                    -1.0f
                    + 2.0f * static_cast<float>(X) / LengthSegments;
                const float LocalX = CenterX + Along * HalfLength;
                const float LocalY = Across * CoveredHalfWidth;
                const float FullBucketU = FMath::Clamp(
                    (
                        LocalX
                        + BucketInteriorLengthCentimeters * 0.5f
                    )
                    / BucketInteriorLengthCentimeters,
                    0.0f,
                    1.0f
                );
                const float BottomZ =
                    BottomHeight(FullBucketU, Across);
                float LocalZ = BottomZ;
                if (bTopSurface)
                {
                    float PlaneZ =
                        PlaneCenterZ
                        - (
                            GravityUpLocal.X
                                * (LocalX - CenterX)
                            + GravityUpLocal.Y * LocalY
                        )
                        / GravityUpLocal.Z;
                    const float MoundProfile = FMath::Pow(
                        FMath::Max(
                            FMath::Sin(
                                PI * static_cast<float>(X)
                                / LengthSegments
                            )
                            * FMath::Sin(
                                PI * static_cast<float>(Y)
                                / WidthSegments
                            ),
                            0.0f
                        ),
                        0.62f
                    );
                    const float HighFillAmount =
                        FMath::SmoothStep(0.65f, 1.0f, FillRatio);
                    const float Mound =
                        BucketHeapHeightCentimeters
                        * FillRatio
                        * FMath::Lerp(
                            0.72f,
                            1.0f,
                            HighFillAmount
                        )
                        * MoundProfile;
                    const float GrainVariation =
                        1.4f
                        * FMath::Sin(
                            static_cast<float>(X) * 2.17f
                            + static_cast<float>(Y) * 1.31f
                        );
                    LocalZ = FMath::Clamp(
                        PlaneZ + Mound + GrainVariation,
                        BottomZ + 1.5f,
                        MaximumSurfaceHeight(FullBucketU)
                            + BucketHeapHeightCentimeters
                    );
                }

                LoadVertices.Add(FVector(LocalX, LocalY, LocalZ));
                LoadUVs.Add(
                    FVector2D(
                        static_cast<float>(X) / LengthSegments,
                        static_cast<float>(Y) / WidthSegments
                    )
                    * 1.8f
                );
                const float Fleck =
                    0.88f
                    + 0.12f
                        * FMath::Sin(
                            static_cast<float>(X) * 1.73f
                            + static_cast<float>(Y) * 2.41f
                        );
                FLinearColor LoadColor =
                    FLinearColor(1.22f, 0.91f, 0.70f, 1.0f)
                    * Fleck;
                LoadColor.A = 1.0f;
                LoadColors.Add(LoadColor);
            }
        }
    }

    for (int32 Y = 0; Y < WidthSegments; ++Y)
    {
        for (int32 X = 0; X < LengthSegments; ++X)
        {
            const int32 V00 = Y * RowSize + X;
            const int32 V10 = V00 + 1;
            const int32 V01 = V00 + RowSize;
            const int32 V11 = V01 + 1;

            LoadTriangles.Append({V00, V11, V10, V00, V01, V11});

            const int32 B00 = V00 + SurfaceVertexCount;
            const int32 B10 = V10 + SurfaceVertexCount;
            const int32 B01 = V01 + SurfaceVertexCount;
            const int32 B11 = V11 + SurfaceVertexCount;
            LoadTriangles.Append({B00, B10, B11, B00, B11, B01});
        }
    }

    const auto AddSideQuad = [&LoadTriangles, SurfaceVertexCount](
        const int32 TopA,
        const int32 TopB
    )
    {
        const int32 BottomA = TopA + SurfaceVertexCount;
        const int32 BottomB = TopB + SurfaceVertexCount;
        LoadTriangles.Append(
            {
                TopA,
                TopB,
                BottomB,
                TopA,
                BottomB,
                BottomA
            }
        );
    };

    for (int32 X = 0; X < LengthSegments; ++X)
    {
        AddSideQuad(X + 1, X);
        const int32 FarRow = WidthSegments * RowSize;
        AddSideQuad(FarRow + X, FarRow + X + 1);
    }
    for (int32 Y = 0; Y < WidthSegments; ++Y)
    {
        AddSideQuad(Y * RowSize, (Y + 1) * RowSize);
        AddSideQuad(
            (Y + 1) * RowSize + LengthSegments,
            Y * RowSize + LengthSegments
        );
    }

    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
        LoadVertices,
        LoadTriangles,
        LoadUVs,
        LoadNormals,
        LoadTangents
    );
    BucketSoilMesh->ClearAllMeshSections();
    BucketSoilMesh->CreateMeshSection_LinearColor(
        0,
        LoadVertices,
        LoadTriangles,
        LoadNormals,
        LoadUVs,
        LoadColors,
        LoadTangents,
        false,
        false
    );
    if (GranularParticleMaterial)
    {
        BucketSoilMesh->SetMaterial(0, GranularParticleMaterial);
    }
    else if (SoilMaterial)
    {
        BucketSoilMesh->SetMaterial(0, SoilMaterial);
    }
}

void ADiggableTerrain::InitializeGranularSoil()
{
    if (
        !bEnableGranularSoil
        || bGranularInitialized
        || !ExcavatorMesh.IsValid()
        || !GranularSoilInstances
        || !GranularSoilInstances->GetStaticMesh()
        || CurrentHeights.IsEmpty()
    )
    {
        return;
    }

    const FTransform BucketTransform =
        BucketSoilMesh
        ? BucketSoilMesh->GetComponentTransform()
        : ExcavatorMesh->GetSocketTransform(
            BucketBoneName,
            RTS_World
        );
    const FVector BucketTipWorld =
        BucketTransform.TransformPosition(
            FVector(
                BucketInteriorLengthCentimeters * 0.5f + 9.0f,
                0.0f,
                -BucketInteriorDepthCentimeters * 0.32f
            )
        );
    const FVector BucketTipLocal =
        GetActorTransform().InverseTransformPosition(
            BucketTipWorld
        );
    GranularPatchCenterLocal = FVector2D(BucketTipLocal);

    const float Radius = FMath::Max(
        GranularParticleRadiusCentimeters,
        1.0f
    );
    const float Diameter = Radius * 2.0f;
    const float HorizontalSpacing = Diameter * 1.02f;
    const float VerticalSpacing = Diameter * 0.98f;
    const int32 CountX = FMath::Max(
        1,
        FMath::FloorToInt(
            GranularPatchLengthCentimeters / HorizontalSpacing
        )
    );
    const int32 CountY = FMath::Max(
        1,
        FMath::FloorToInt(
            GranularPatchWidthCentimeters / HorizontalSpacing
        )
    );
    const int32 CountZ = FMath::Max(
        1,
        FMath::FloorToInt(
            GranularBedDepthCentimeters / VerticalSpacing
        )
    );
    const int32 ClusterCountX = FMath::DivideAndRoundUp(CountX, 3);
    const int32 ClusterCountY = FMath::DivideAndRoundUp(CountY, 3);

    GranularParticles.Reset();
    GranularParticles.Reserve(
        FMath::Min(CountX * CountY * CountZ, MaximumGranularParticles)
    );
    GranularSoilInstances->ClearInstances();

    FRandomStream Random(24072026);
    const FVector2D PatchMinimum =
        GranularPatchCenterLocal
        - FVector2D(
            (CountX - 1) * HorizontalSpacing * 0.5f,
            (CountY - 1) * HorizontalSpacing * 0.5f
        );
    bool bReachedParticleLimit = false;
    for (int32 Z = 0; Z < CountZ && !bReachedParticleLimit; ++Z)
    {
        for (int32 Y = 0; Y < CountY && !bReachedParticleLimit; ++Y)
        {
            const float RowOffset =
                ((Y + Z) & 1) != 0
                ? HorizontalSpacing * 0.5f
                : 0.0f;
            for (int32 X = 0; X < CountX; ++X)
            {
                if (
                    GranularParticles.Num()
                    >= MaximumGranularParticles
                )
                {
                    bReachedParticleLimit = true;
                    break;
                }

                FVector LocalPosition(
                    PatchMinimum.X
                        + X * HorizontalSpacing
                        + RowOffset,
                    PatchMinimum.Y + Y * HorizontalSpacing,
                    0.0f
                );
                const FVector2D HorizontalPosition(LocalPosition);
                if (!IsInsideTerrain(HorizontalPosition))
                {
                    continue;
                }
                LocalPosition.Z =
                    SampleHeight(HorizontalPosition)
                    + Radius
                    + Z * VerticalSpacing;
                LocalPosition.X += Random.FRandRange(
                    -Radius * 0.08f,
                    Radius * 0.08f
                );
                LocalPosition.Y += Random.FRandRange(
                    -Radius * 0.08f,
                    Radius * 0.08f
                );

                FGranularParticle& Particle =
                    GranularParticles.AddDefaulted_GetRef();
                Particle.Position =
                    GetActorTransform().TransformPosition(LocalPosition);
                Particle.PreviousPosition = Particle.Position;
                Particle.CohesionCluster =
                    (X / 3)
                    + (Y / 3) * ClusterCountX
                    + (Z / 2) * ClusterCountX * ClusterCountY;
                const float OverallScale =
                    Random.FRandRange(0.86f, 1.10f);
                Particle.VisualScale = FVector(
                    OverallScale * Random.FRandRange(0.72f, 1.24f),
                    OverallScale * Random.FRandRange(0.72f, 1.24f),
                    OverallScale * Random.FRandRange(0.68f, 1.12f)
                );
                Particle.VisualRotation = FQuat(
                    FRotator(
                        Random.FRandRange(-180.0f, 180.0f),
                        Random.FRandRange(-180.0f, 180.0f),
                        Random.FRandRange(-180.0f, 180.0f)
                    )
                );
            }
        }
    }

    ActiveGranularParticleCount = GranularParticles.Num();
    GranularInstanceTransforms.SetNum(GranularParticles.Num());
    const float ReferenceRadius =
        GranularSoilInstances->GetStaticMesh()
        ? FMath::Max(
            GranularSoilInstances
                ->GetStaticMesh()
                ->GetBounds()
                .BoxExtent
                .GetMax(),
            1.0f
        )
        : 50.0f;
    const float BaseVisualScale = Radius / ReferenceRadius;
    const FTransform ActorTransform = GetActorTransform();
    for (int32 Index = 0; Index < GranularParticles.Num(); ++Index)
    {
        const FGranularParticle& Particle = GranularParticles[Index];
        GranularInstanceTransforms[Index] = FTransform(
            Particle.VisualRotation,
            ActorTransform.InverseTransformPosition(Particle.Position),
            Particle.VisualScale * BaseVisualScale
        );
    }
    GranularSoilInstances->AddInstances(
        GranularInstanceTransforms,
        false,
        false,
        false
    );
    if (GranularParticleMaterialBase)
    {
        GranularParticleMaterial =
            UMaterialInstanceDynamic::Create(
                GranularParticleMaterialBase,
                this
            );
        GranularParticleMaterial->SetVectorParameterValue(
            TEXT("Color"),
            FLinearColor(0.42f, 0.065f, 0.018f, 1.0f)
        );
        GranularParticleMaterial->SetScalarParameterValue(
            TEXT("Roughness"),
            0.94f
        );
        GranularSoilInstances->SetMaterial(
            0,
            GranularParticleMaterial
        );
    }
    else if (SoilMaterial)
    {
        GranularSoilInstances->SetMaterial(0, SoilMaterial);
    }
    GranularSoilInstances->SetVisibility(true, true);
    GranularSoilInstances->SetHiddenInGame(false);

    bGranularInitialized = true;
    bHasPreviousBucketCavityTransform = false;
    bHasGranularActivationTransform = false;
    GranularSimulationAccumulator = 0.0f;
    GranularRenderAccumulator = 0.0f;
    GranularBenchmarkSeconds = 0.0;
    GranularBenchmarkSteps = 0;
    bGranularTransformsDirty = false;
    UpdateBucketLoadVisual();

    UE_LOG(
        LogDiggableTerrain,
        Log,
        TEXT(
            "Initialized granular soil with %d particles "
            "(radius %.1f cm)"
        ),
        ActiveGranularParticleCount,
        Radius
    );
}

void ADiggableTerrain::InitializeSurfaceClumps()
{
    if (
        !bEnableSurfaceClumps
        || bSurfaceClumpsInitialized
        || !SurfaceClumpInstances
        || !SurfaceClumpInstances->GetStaticMesh()
        || CurrentHeights.IsEmpty()
    )
    {
        return;
    }

    const float HalfExtent = FMath::Min(
        SurfaceClumpHalfExtentCentimeters,
        GridResolution * CellSizeCentimeters * 0.5f - 100.0f
    );
    const float Spacing = FMath::Max(
        SurfaceClumpSpacingCentimeters,
        18.0f
    );
    const int32 Count = FMath::FloorToInt(
        HalfExtent * 2.0f / Spacing
    );
    if (Count <= 0)
    {
        return;
    }

    const FBoxSphereBounds MeshBounds =
        SurfaceClumpInstances->GetStaticMesh()->GetBounds();
    const FVector MeshExtent(
        FMath::Max(MeshBounds.BoxExtent.X, 1.0f),
        FMath::Max(MeshBounds.BoxExtent.Y, 1.0f),
        FMath::Max(MeshBounds.BoxExtent.Z, 1.0f)
    );
    const FVector2D DynamicHalfSize(
        GranularPatchLengthCentimeters * 0.5f + 90.0f,
        GranularPatchWidthCentimeters * 0.5f + 90.0f
    );

    TArray<FTransform> Instances;
    Instances.Reserve(Count * Count);
    FRandomStream Random(24072027);
    for (int32 Y = 0; Y < Count; ++Y)
    {
        for (int32 X = 0; X < Count; ++X)
        {
            FVector2D Position(
                -HalfExtent + (X + 0.5f) * Spacing,
                -HalfExtent + (Y + 0.5f) * Spacing
            );
            Position.X += Random.FRandRange(
                -Spacing * 0.38f,
                Spacing * 0.38f
            );
            Position.Y += Random.FRandRange(
                -Spacing * 0.38f,
                Spacing * 0.38f
            );

            const FVector2D FromDynamicPatch =
                Position - GranularPatchCenterLocal;
            if (
                FMath::Abs(FromDynamicPatch.X) < DynamicHalfSize.X
                && FMath::Abs(FromDynamicPatch.Y) < DynamicHalfSize.Y
            )
            {
                continue;
            }
            if (!IsInsideTerrain(Position))
            {
                continue;
            }

            const float HorizontalExtent =
                Random.FRandRange(7.0f, 14.0f);
            const float HeightExtent =
                Random.FRandRange(2.2f, 5.2f);
            const FVector Scale(
                HorizontalExtent
                    * Random.FRandRange(0.78f, 1.24f)
                    / MeshExtent.X,
                HorizontalExtent
                    * Random.FRandRange(0.78f, 1.24f)
                    / MeshExtent.Y,
                HeightExtent / MeshExtent.Z
            );
            const FVector LocalLocation(
                Position.X,
                Position.Y,
                SampleHeight(Position) - HeightExtent * 0.45f
            );
            Instances.Emplace(
                FRotator(
                    Random.FRandRange(-8.0f, 8.0f),
                    Random.FRandRange(-180.0f, 180.0f),
                    Random.FRandRange(-8.0f, 8.0f)
                ),
                LocalLocation,
                Scale
            );
        }
    }

    SurfaceClumpInstances->ClearInstances();
    SurfaceClumpInstances->AddInstances(
        Instances,
        false,
        false,
        false
    );
    if (GranularParticleMaterial)
    {
        SurfaceClumpInstances->SetMaterial(
            0,
            GranularParticleMaterial
        );
    }
    else if (SoilMaterial)
    {
        SurfaceClumpInstances->SetMaterial(0, SoilMaterial);
    }
    SurfaceClumpInstances->SetVisibility(true, true);
    SurfaceClumpInstances->SetHiddenInGame(false);
    bSurfaceClumpsInitialized = true;

    UE_LOG(
        LogDiggableTerrain,
        Log,
        TEXT("Initialized %d sleeping surface clumps"),
        Instances.Num()
    );
}

void ADiggableTerrain::ResetGranularSoil()
{
    GranularParticles.Reset();
    GranularNextInCell.Reset();
    GranularCellHeads.Reset();
    GranularInstanceTransforms.Reset();
    ActiveGranularParticleCount = 0;
    bGranularInitialized = false;
    bHasPreviousBucketCavityTransform = false;
    bHasGranularActivationTransform = false;
    GranularSimulationAccumulator = 0.0f;
    GranularRenderAccumulator = 0.0f;
    bGranularTransformsDirty = false;
    if (GranularSoilInstances)
    {
        GranularSoilInstances->ClearInstances();
        GranularSoilInstances->SetVisibility(
            bEnableGranularSoil,
            true
        );
        GranularSoilInstances->SetHiddenInGame(!bEnableGranularSoil);
    }
    InitializeGranularSoil();
}

void ADiggableTerrain::TickGranularSoil(const float DeltaTime)
{
    if (!bEnableGranularSoil)
    {
        if (GranularSoilInstances)
        {
            GranularSoilInstances->SetVisibility(false, true);
            GranularSoilInstances->SetHiddenInGame(true);
        }
        return;
    }

    if (!bGranularInitialized)
    {
        InitializeGranularSoil();
    }
    if (!bGranularInitialized || GranularParticles.IsEmpty())
    {
        return;
    }

    constexpr float FixedDeltaTime = 1.0f / 60.0f;
    GranularSimulationAccumulator = FMath::Min(
        GranularSimulationAccumulator
            + FMath::Min(DeltaTime, 0.05f),
        FixedDeltaTime * 3.0f
    );
    int32 SubstepCount = 0;
    while (
        GranularSimulationAccumulator >= FixedDeltaTime
        && SubstepCount < 3
    )
    {
        StepGranularSoil(FixedDeltaTime);
        GranularSimulationAccumulator -= FixedDeltaTime;
        ++SubstepCount;
    }

    GranularRenderAccumulator += DeltaTime;
    if (GranularRenderAccumulator >= 1.0f / 20.0f)
    {
        GranularRenderAccumulator = 0.0f;
        UpdateGranularInstanceTransforms();
    }
}

void ADiggableTerrain::StepGranularSoil(
    const float FixedDeltaTime
)
{
    const double StartSeconds = FPlatformTime::Seconds();
    const FTransform BucketCavityTransform =
        BucketSoilMesh
        ? BucketSoilMesh->GetComponentTransform()
        : FTransform::Identity;
    const FTransform PreviousCavityTransform =
        bHasPreviousBucketCavityTransform
        ? PreviousBucketCavityTransform
        : BucketCavityTransform;
    const FTransform ActivationReference =
        bHasGranularActivationTransform
        ? LastGranularActivationTransform
        : BucketCavityTransform;
    const bool bBucketMoving =
        !bHasGranularActivationTransform
        || FVector::DistSquared(
            BucketCavityTransform.GetLocation(),
            ActivationReference.GetLocation()
        ) > 1.0f
        || (
            1.0f
            - FMath::Abs(
                BucketCavityTransform.GetRotation()
                | ActivationReference.GetRotation()
            )
        ) > 0.00001f;
    if (bBucketMoving)
    {
        LastGranularActivationTransform = BucketCavityTransform;
        bHasGranularActivationTransform = true;
    }
    const FVector BucketCenter = BucketCavityTransform.GetLocation();
    const float ActivationRadius = FMath::Max(
        GranularActivationRadiusCentimeters,
        BucketInteriorLengthCentimeters
    );
    const float ActivationRadiusSquared =
        FMath::Square(ActivationRadius);
    const bool bReleaseCarriedParticles = bWasDumping;
    const FVector BucketPourVelocity =
        BucketCavityTransform.TransformVectorNoScale(
            FVector::ForwardVector
        ).GetSafeNormal()
        * 32.0f
        + FVector::DownVector * 22.0f;
    bool bAnyParticleMoved = false;

    const float Gravity =
        FMath::Max(GranularGravityCentimetersPerSecondSquared, 0.0f);
    for (FGranularParticle& Particle : GranularParticles)
    {
        Particle.bGrounded = false;
        if (Particle.bCarried)
        {
            if (bReleaseCarriedParticles)
            {
                Particle.bCarried = false;
                Particle.bActive = true;
                Particle.SleepSeconds = 0.0f;
                Particle.Velocity = BucketPourVelocity;
                Particle.PreviousPosition = Particle.Position;
                bAnyParticleMoved = true;
            }
            else
            {
                const FVector TargetPosition =
                    BucketCavityTransform.TransformPosition(
                        Particle.CarriedLocalPosition
                    );
                Particle.PreviousPosition = Particle.Position;
                Particle.Velocity =
                    (TargetPosition - Particle.Position)
                    / FixedDeltaTime;
                Particle.Position = TargetPosition;
                bAnyParticleMoved = true;
                continue;
            }
        }

        if (
            bBucketMoving
            &&
            FVector::DistSquared(
                Particle.Position,
                BucketCenter
            ) <= ActivationRadiusSquared
        )
        {
            Particle.bActive = true;
            Particle.SleepSeconds = 0.0f;
        }
        if (!Particle.bActive)
        {
            Particle.PreviousPosition = Particle.Position;
            Particle.Velocity = FVector::ZeroVector;
            continue;
        }

        Particle.PreviousPosition = Particle.Position;
        Particle.Velocity.Z -= Gravity * FixedDeltaTime;
        Particle.Velocity *= 0.992f;
        Particle.Position += Particle.Velocity * FixedDeltaTime;
        bAnyParticleMoved = true;
    }

    if (!bAnyParticleMoved)
    {
        PreviousBucketCavityTransform = BucketCavityTransform;
        bHasPreviousBucketCavityTransform = true;
        return;
    }

    const int32 IterationCount = FMath::Clamp(
        GranularSolverIterations,
        1,
        6
    );
    for (int32 Iteration = 0; Iteration < IterationCount; ++Iteration)
    {
        BuildGranularSpatialHash();
        SolveGranularParticleContacts();
        for (FGranularParticle& Particle : GranularParticles)
        {
            if (!Particle.bActive || Particle.bCarried)
            {
                continue;
            }
            const bool bInsideBucket = SolveGranularBucketContact(
                Particle,
                BucketCavityTransform,
                PreviousCavityTransform,
                FixedDeltaTime
            );
            if (!bInsideBucket)
            {
                SolveGranularTerrainContact(Particle);
            }
        }
    }

    const float MaximumSpeed = 360.0f;
    for (FGranularParticle& Particle : GranularParticles)
    {
        if (!Particle.bActive || Particle.bCarried)
        {
            continue;
        }
        const FVector ConstraintVelocity =
            (Particle.Position - Particle.PreviousPosition)
            / FixedDeltaTime;
        Particle.Velocity = FMath::Lerp(
            Particle.Velocity,
            ConstraintVelocity,
            0.42f
        );
        Particle.Velocity *= 0.982f;
        Particle.Velocity =
            Particle.Velocity.GetClampedToMaxSize(MaximumSpeed);

        const bool bNearMovingBucket =
            bBucketMoving
            &&
            FVector::DistSquared(
                Particle.Position,
                BucketCenter
            ) <= FMath::Square(ActivationRadius * 0.68f);
        if (
            Particle.bGrounded
            && Particle.Velocity.SizeSquared()
                < FMath::Square(7.0f)
            && !bNearMovingBucket
        )
        {
            Particle.SleepSeconds += FixedDeltaTime;
            if (Particle.SleepSeconds >= 0.75f)
            {
                Particle.bActive = false;
                Particle.Velocity = FVector::ZeroVector;
            }
        }
        else
        {
            Particle.SleepSeconds = 0.0f;
        }
    }

    PreviousBucketCavityTransform = BucketCavityTransform;
    bHasPreviousBucketCavityTransform = true;
    bGranularTransformsDirty =
        bGranularTransformsDirty || bAnyParticleMoved;

    GranularBenchmarkSeconds +=
        FPlatformTime::Seconds() - StartSeconds;
    ++GranularBenchmarkSteps;
    if (GranularBenchmarkSteps >= 180)
    {
        UE_LOG(
            LogDiggableTerrain,
            Log,
            TEXT(
                "Granular benchmark: %d particles, %.2f ms/step"
            ),
            GranularParticles.Num(),
            GranularBenchmarkSeconds
                * 1000.0
                / GranularBenchmarkSteps
        );
        GranularBenchmarkSeconds = 0.0;
        GranularBenchmarkSteps = 0;
    }
}

FIntVector ADiggableTerrain::GranularCellForPosition(
    const FVector& Position
) const
{
    const float CellSize = FMath::Max(
        GranularParticleRadiusCentimeters * 2.0f,
        1.0f
    );
    return FIntVector(
        FMath::FloorToInt(Position.X / CellSize),
        FMath::FloorToInt(Position.Y / CellSize),
        FMath::FloorToInt(Position.Z / CellSize)
    );
}

void ADiggableTerrain::BuildGranularSpatialHash()
{
    GranularCellHeads.Reset();
    GranularNextInCell.SetNumUninitialized(GranularParticles.Num());
    for (int32 Index = 0; Index < GranularParticles.Num(); ++Index)
    {
        const FIntVector Cell =
            GranularCellForPosition(
                GranularParticles[Index].Position
            );
        int32 PreviousHead = INDEX_NONE;
        if (const int32* Head = GranularCellHeads.Find(Cell))
        {
            PreviousHead = *Head;
        }
        GranularNextInCell[Index] = PreviousHead;
        GranularCellHeads.FindOrAdd(Cell) = Index;
    }
}

void ADiggableTerrain::SolveGranularParticleContacts()
{
    const float Diameter =
        FMath::Max(GranularParticleRadiusCentimeters * 2.0f, 1.0f);
    const float DiameterSquared = FMath::Square(Diameter);
    const float CohesionRange =
        Diameter
        * FMath::Clamp(
            GranularCohesionRangeMultiplier,
            1.0f,
            1.6f
        );
    const float CohesionRangeSquared =
        FMath::Square(CohesionRange);
    const float Stiffness = FMath::Clamp(
        GranularContactStiffness,
        0.0f,
        1.0f
    );
    const float CohesionStrength = FMath::Clamp(
        GranularCohesionStrength,
        0.0f,
        1.0f
    );

    for (int32 Index = 0; Index < GranularParticles.Num(); ++Index)
    {
        FGranularParticle& Particle = GranularParticles[Index];
        if (!Particle.bActive || Particle.bCarried)
        {
            continue;
        }
        const FIntVector Cell =
            GranularCellForPosition(Particle.Position);
        for (int32 Z = -1; Z <= 1; ++Z)
        {
            for (int32 Y = -1; Y <= 1; ++Y)
            {
                for (int32 X = -1; X <= 1; ++X)
                {
                    const FIntVector NeighborCell =
                        Cell + FIntVector(X, Y, Z);
                    const int32* Head =
                        GranularCellHeads.Find(NeighborCell);
                    if (!Head)
                    {
                        continue;
                    }

                    for (
                        int32 NeighborIndex = *Head;
                        NeighborIndex != INDEX_NONE;
                        NeighborIndex =
                            GranularNextInCell[NeighborIndex]
                    )
                    {
                        if (NeighborIndex == Index)
                        {
                            continue;
                        }

                        FGranularParticle& Neighbor =
                            GranularParticles[NeighborIndex];
                        if (
                            Neighbor.bActive
                            && NeighborIndex < Index
                        )
                        {
                            continue;
                        }
                        if (
                            Neighbor.bCarried
                            || (
                                !Particle.bActive
                                && !Neighbor.bActive
                            )
                        )
                        {
                            continue;
                        }
                        const bool bCohesivePair =
                            CohesionStrength > KINDA_SMALL_NUMBER
                            && Particle.CohesionCluster
                                == Neighbor.CohesionCluster;
                        FVector Separation =
                            Neighbor.Position - Particle.Position;
                        float DistanceSquared =
                            Separation.SizeSquared();
                        const float InteractionDistanceSquared =
                            bCohesivePair
                            ? CohesionRangeSquared
                            : DiameterSquared;
                        if (
                            DistanceSquared
                            >= InteractionDistanceSquared
                        )
                        {
                            continue;
                        }

                        if (DistanceSquared < 0.0001f)
                        {
                            Separation = FVector(
                                ((Index + NeighborIndex) & 1) != 0
                                    ? 1.0f
                                    : -1.0f,
                                0.0f,
                                0.0f
                            );
                            DistanceSquared = 1.0f;
                        }
                        const float Distance =
                            FMath::Sqrt(DistanceSquared);
                        const FVector Normal = Separation / Distance;
                        Particle.bActive = true;
                        Neighbor.bActive = true;
                        Particle.SleepSeconds = 0.0f;
                        Neighbor.SleepSeconds = 0.0f;

                        if (Distance < Diameter)
                        {
                            const FVector Correction =
                                Normal
                                * (Diameter - Distance)
                                * 0.5f
                                * Stiffness;
                            Particle.Position -= Correction;
                            Neighbor.Position += Correction;
                        }
                        else if (bCohesivePair)
                        {
                            const float Stretch =
                                Distance - Diameter * 0.98f;
                            const float MaximumPull =
                                Diameter * 0.055f;
                            const FVector Pull =
                                Normal
                                * FMath::Min(
                                    Stretch
                                        * 0.5f
                                        * CohesionStrength,
                                    MaximumPull
                                );
                            Particle.Position += Pull;
                            Neighbor.Position -= Pull;
                        }

                        const FVector RelativeTravel =
                            (
                                Neighbor.Position
                                - Neighbor.PreviousPosition
                            )
                            - (
                                Particle.Position
                                - Particle.PreviousPosition
                            );
                        const FVector TangentialTravel =
                            RelativeTravel
                            - Normal
                                * FVector::DotProduct(
                                    RelativeTravel,
                                    Normal
                                );
                        const FVector FrictionCorrection =
                            TangentialTravel
                            * GranularSurfaceFriction
                            * 0.035f;
                        Particle.Position += FrictionCorrection;
                        Neighbor.Position -= FrictionCorrection;
                    }
                }
            }
        }
    }
}

bool ADiggableTerrain::SolveGranularBucketContact(
    FGranularParticle& Particle,
    const FTransform& BucketCavityTransform,
    const FTransform& PreviousCavityTransform,
    const float FixedDeltaTime
)
{
    if (!ExcavatorMesh.IsValid() || !BucketSoilMesh)
    {
        return false;
    }

    const float Radius = GranularParticleRadiusCentimeters;
    const float HalfLength = BucketInteriorLengthCentimeters * 0.5f;
    const float HalfWidth = BucketInteriorWidthCentimeters * 0.5f;
    const float Depth = BucketInteriorDepthCentimeters;
    FVector LocalPosition =
        BucketCavityTransform.InverseTransformPosition(
            Particle.Position
        );

    const float FullBucketU = FMath::Clamp(
        (LocalPosition.X + HalfLength)
            / FMath::Max(BucketInteriorLengthCentimeters, 1.0f),
        0.0f,
        1.0f
    );
    const float Longitudinal = 2.0f * FullBucketU - 1.0f;
    const float Across = FMath::Clamp(
        LocalPosition.Y / FMath::Max(HalfWidth, 1.0f),
        -1.0f,
        1.0f
    );
    const float BottomHeight =
        -Depth * 0.42f
        + Depth * 0.28f * FMath::Square(Longitudinal)
        - Depth * 0.18f * FullBucketU
        + Depth * 0.07f * FMath::Square(Across);
    const float TopHeight =
        Depth * 0.20f
        - Depth * 0.16f * FullBucketU
        + BucketHeapHeightCentimeters
        + Radius * 2.0f;

    const bool bNearBucket =
        LocalPosition.X >= -HalfLength - Radius * 2.0f
        && LocalPosition.X <= HalfLength + Radius * 4.0f
        && FMath::Abs(LocalPosition.Y)
            <= HalfWidth + Radius * 1.8f
        && LocalPosition.Z >= BottomHeight - Radius * 3.0f
        && LocalPosition.Z <= TopHeight + Radius * 3.0f;
    if (!bNearBucket)
    {
        return false;
    }

    const float CurlAmount = ExcavatorAdapter.IsValid()
        ? FMath::Clamp(
            -ExcavatorAdapter->GetBucketInput(),
            0.0f,
            1.0f
        )
        : 0.0f;
    const bool bAtMouth =
        LocalPosition.X > HalfLength - Radius * 2.5f
        && LocalPosition.X < HalfLength + Radius * 4.0f
        && FMath::Abs(LocalPosition.Y)
            < HalfWidth - Radius * 0.35f
        && LocalPosition.Z > BottomHeight - Radius
        && LocalPosition.Z < TopHeight + Radius * 2.0f;
    if (
        CurlAmount > 0.05f
        && bAtMouth
    )
    {
        const FVector CaptureTarget(
            HalfLength * 0.12f,
            LocalPosition.Y * 0.80f,
            BottomHeight + Radius * 1.45f
        );
        const FVector ToTarget = CaptureTarget - LocalPosition;
        LocalPosition += ToTarget.GetClampedToMaxSize(
            145.0f * CurlAmount * FixedDeltaTime
        );
        Particle.Position =
            BucketCavityTransform.TransformPosition(LocalPosition);
    }

    bool bContact = false;
    const bool bWithinLength =
        LocalPosition.X >= -HalfLength - Radius
        && LocalPosition.X <= HalfLength + Radius * 1.5f;
    const bool bWithinWidth =
        FMath::Abs(LocalPosition.Y) <= HalfWidth + Radius * 1.5f;
    if (bWithinLength && bWithinWidth)
    {
        if (
            LocalPosition.Z < BottomHeight + Radius
            && LocalPosition.Z > BottomHeight - Radius * 6.0f
        )
        {
            LocalPosition.Z = BottomHeight + Radius;
            bContact = true;
        }

        if (
            LocalPosition.X < -HalfLength + Radius
            && LocalPosition.X > -HalfLength - Radius * 3.0f
            && LocalPosition.Z < TopHeight
        )
        {
            LocalPosition.X = -HalfLength + Radius;
            bContact = true;
        }

        const float SideLimit = HalfWidth - Radius;
        if (
            FMath::Abs(LocalPosition.Y) > SideLimit
            && FMath::Abs(LocalPosition.Y)
                < HalfWidth + Radius * 3.0f
            && LocalPosition.Z < TopHeight
        )
        {
            LocalPosition.Y =
                FMath::Sign(LocalPosition.Y) * SideLimit;
            bContact = true;
        }
    }

    const bool bInsideCavity =
        LocalPosition.X >= -HalfLength
        && LocalPosition.X <= HalfLength
        && FMath::Abs(LocalPosition.Y) <= HalfWidth
        && LocalPosition.Z >= BottomHeight - Radius
        && LocalPosition.Z <= TopHeight;
    if (bContact || bInsideCavity)
    {
        const FVector CorrectedWorldPosition =
            BucketCavityTransform.TransformPosition(LocalPosition);
        const FVector PreviousWorldAtLocalPoint =
            PreviousCavityTransform.TransformPosition(LocalPosition);
        const FVector BucketMovement =
            CorrectedWorldPosition - PreviousWorldAtLocalPoint;
        Particle.Position =
            CorrectedWorldPosition
            + BucketMovement * (bContact ? 0.30f : 0.05f);
        if (bContact)
        {
            Particle.bGrounded = true;
            const FVector RelativeMovement =
                Particle.Position - Particle.PreviousPosition;
            Particle.Position -= FVector(
                RelativeMovement.X,
                RelativeMovement.Y,
                0.0f
            ) * GranularSurfaceFriction * 0.08f;
        }
        if (
            CurlAmount > 0.06f
            && (bAtMouth || bInsideCavity)
        )
        {
            LocalPosition.X = FMath::Clamp(
                LocalPosition.X,
                -HalfLength + Radius * 1.4f,
                HalfLength - Radius * 1.4f
            );
            LocalPosition.Y = FMath::Clamp(
                LocalPosition.Y,
                -HalfWidth + Radius * 1.4f,
                HalfWidth - Radius * 1.4f
            );
            LocalPosition.Z = FMath::Clamp(
                LocalPosition.Z,
                BottomHeight + Radius,
                TopHeight - Radius * 0.5f
            );
            Particle.CarriedLocalPosition = LocalPosition;
            Particle.bCarried = true;
            Particle.bActive = true;
            Particle.SleepSeconds = 0.0f;
            Particle.Velocity = FVector::ZeroVector;
            Particle.Position =
                BucketCavityTransform.TransformPosition(
                    Particle.CarriedLocalPosition
                );
        }
        return bInsideCavity || bContact;
    }
    return false;
}

void ADiggableTerrain::SolveGranularTerrainContact(
    FGranularParticle& Particle
)
{
    const FTransform ActorTransform = GetActorTransform();
    FVector LocalPosition =
        ActorTransform.InverseTransformPosition(Particle.Position);
    const FVector PreviousLocalPosition =
        ActorTransform.InverseTransformPosition(
            Particle.PreviousPosition
        );
    const float Radius = GranularParticleRadiusCentimeters;
    const FVector2D HorizontalPosition(LocalPosition);
    if (IsInsideTerrain(HorizontalPosition))
    {
        const float GroundHeight =
            SampleHeight(HorizontalPosition) + Radius;
        if (LocalPosition.Z < GroundHeight)
        {
            LocalPosition.Z = GroundHeight;
            Particle.bGrounded = true;
            const FVector LocalTravel =
                LocalPosition - PreviousLocalPosition;
            LocalPosition.X -=
                LocalTravel.X * GranularSurfaceFriction;
            LocalPosition.Y -=
                LocalTravel.Y * GranularSurfaceFriction;
            if (LocalTravel.Z < 0.0f)
            {
                Particle.Velocity.Z = 0.0f;
            }
        }
    }

    const float SpillMargin = 180.0f;
    const float MinimumX =
        GranularPatchCenterLocal.X
        - GranularPatchLengthCentimeters * 0.5f
        - SpillMargin;
    const float MaximumX =
        GranularPatchCenterLocal.X
        + GranularPatchLengthCentimeters * 0.5f
        + SpillMargin;
    const float MinimumY =
        GranularPatchCenterLocal.Y
        - GranularPatchWidthCentimeters * 0.5f
        - SpillMargin;
    const float MaximumY =
        GranularPatchCenterLocal.Y
        + GranularPatchWidthCentimeters * 0.5f
        + SpillMargin;
    LocalPosition.X = FMath::Clamp(
        LocalPosition.X,
        MinimumX,
        MaximumX
    );
    LocalPosition.Y = FMath::Clamp(
        LocalPosition.Y,
        MinimumY,
        MaximumY
    );
    Particle.Position =
        ActorTransform.TransformPosition(LocalPosition);
}

void ADiggableTerrain::UpdateGranularInstanceTransforms()
{
    if (
        !GranularSoilInstances
        || !bGranularTransformsDirty
        || GranularParticles.IsEmpty()
        || GranularSoilInstances->GetInstanceCount()
            != GranularParticles.Num()
    )
    {
        return;
    }

    GranularInstanceTransforms.SetNum(GranularParticles.Num());
    const float ReferenceRadius =
        GranularSoilInstances->GetStaticMesh()
        ? FMath::Max(
            GranularSoilInstances
                ->GetStaticMesh()
                ->GetBounds()
                .BoxExtent
                .GetMax(),
            1.0f
        )
        : 50.0f;
    const float BaseVisualScale =
        GranularParticleRadiusCentimeters / ReferenceRadius;
    const FTransform ActorTransform = GetActorTransform();
    for (int32 Index = 0; Index < GranularParticles.Num(); ++Index)
    {
        const FGranularParticle& Particle = GranularParticles[Index];
        GranularInstanceTransforms[Index] = FTransform(
            Particle.VisualRotation,
            ActorTransform.InverseTransformPosition(Particle.Position),
            Particle.VisualScale * BaseVisualScale
        );
    }
    GranularSoilInstances->BatchUpdateInstancesTransforms(
        0,
        GranularInstanceTransforms,
        false,
        true,
        true
    );
    bGranularTransformsDirty = false;
}

void ADiggableTerrain::TickDumpStream(const float DeltaTime)
{
    if (DumpStreamParticles.IsEmpty())
    {
        return;
    }

    const float Step = FMath::Min(DeltaTime, 0.05f);
    const float HalfExtent =
        GridResolution * CellSizeCentimeters * 0.5f
        - CellSizeCentimeters;
    bool bDepositedSoil = false;
    for (int32 Index = DumpStreamParticles.Num() - 1;
         Index >= 0;
         --Index)
    {
        FDumpStreamParticle& Particle = DumpStreamParticles[Index];
        Particle.AgeSeconds += Step;
        Particle.Velocity.Z -=
            GranularGravityCentimetersPerSecondSquared * Step;
        Particle.Velocity.X *= 0.995f;
        Particle.Velocity.Y *= 0.995f;
        Particle.Position += Particle.Velocity * Step;
        Particle.Rotation *= FQuat(
            FVector(0.37f, 0.63f, 0.41f).GetSafeNormal(),
            Step * 4.0f
        );

        FVector2D ImpactPosition(Particle.Position);
        const bool bInside = IsInsideTerrain(ImpactPosition);
        const bool bHitGround =
            bInside
            && Particle.Position.Z
                <= SampleHeight(ImpactPosition) + 1.5f;
        const bool bExpired = Particle.AgeSeconds >= 3.0f;
        if (!bHitGround && !bExpired)
        {
            continue;
        }

        ImpactPosition.X = FMath::Clamp(
            ImpactPosition.X,
            -HalfExtent,
            HalfExtent
        );
        ImpactPosition.Y = FMath::Clamp(
            ImpactPosition.Y,
            -HalfExtent,
            HalfExtent
        );
        DepositSpoil(
            ImpactPosition,
            Particle.VolumeCubicCentimeters
        );
        DumpStreamParticles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
        bDepositedSoil = true;
    }

    if (bDepositedSoil)
    {
        UpdateTerrainMesh();
    }
    UpdateDumpStreamInstances();
}

void ADiggableTerrain::SpawnDumpStream(
    const FVector& LocalBucketTip,
    const FVector2D& LocalBucketForward,
    const float VolumeCubicCentimeters
)
{
    if (
        VolumeCubicCentimeters <= KINDA_SMALL_NUMBER
        || !DumpStreamInstances
    )
    {
        return;
    }

    FVector2D Forward = LocalBucketForward.GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
        Forward = FVector2D(1.0f, 0.0f);
    }
    const FVector2D Across(-Forward.Y, Forward.X);
    const int32 ParticleCount = FMath::Clamp(
        FMath::CeilToInt(VolumeCubicCentimeters / 500.0f),
        4,
        32
    );
    const int32 MaximumStreamParticles = 900;
    const int32 AvailableParticles = FMath::Max(
        MaximumStreamParticles - DumpStreamParticles.Num(),
        1
    );
    const int32 SpawnCount = FMath::Min(
        ParticleCount,
        AvailableParticles
    );
    const float CorrectedParticleVolume =
        VolumeCubicCentimeters / SpawnCount;

    for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
    {
        FDumpStreamParticle& Particle =
            DumpStreamParticles.AddDefaulted_GetRef();
        const float AcrossOffset = FMath::FRandRange(
            -BucketInteriorWidthCentimeters * 0.38f,
            BucketInteriorWidthCentimeters * 0.38f
        );
        Particle.Position =
            LocalBucketTip
            + FVector(
                Forward.X * 4.0f + Across.X * AcrossOffset,
                Forward.Y * 4.0f + Across.Y * AcrossOffset,
                FMath::FRandRange(-1.0f, 3.0f)
            );
        Particle.Velocity = FVector(
            Forward.X * FMath::FRandRange(18.0f, 38.0f)
                + Across.X * FMath::FRandRange(-8.0f, 8.0f),
            Forward.Y * FMath::FRandRange(18.0f, 38.0f)
                + Across.Y * FMath::FRandRange(-8.0f, 8.0f),
            FMath::FRandRange(-32.0f, -12.0f)
        );
        Particle.Rotation = FQuat(
            FRotator(
                FMath::FRandRange(-180.0f, 180.0f),
                FMath::FRandRange(-180.0f, 180.0f),
                FMath::FRandRange(-180.0f, 180.0f)
            )
        );
        const float DesiredRadius = FMath::FRandRange(2.2f, 4.4f);
        const float ReferenceRadius =
            DumpStreamInstances->GetStaticMesh()
            ? FMath::Max(
                DumpStreamInstances
                    ->GetStaticMesh()
                    ->GetBounds()
                    .BoxExtent
                    .GetMax(),
                1.0f
            )
            : 50.0f;
        const float Scale = DesiredRadius / ReferenceRadius;
        Particle.VisualScale = FVector(
            Scale * FMath::FRandRange(0.75f, 1.35f),
            Scale * FMath::FRandRange(0.75f, 1.35f),
            Scale * FMath::FRandRange(0.65f, 1.15f)
        );
        Particle.VolumeCubicCentimeters =
            CorrectedParticleVolume;
    }
    UpdateDumpStreamInstances();
}

void ADiggableTerrain::UpdateDumpStreamInstances()
{
    if (!DumpStreamInstances)
    {
        return;
    }
    DumpStreamInstances->ClearInstances();
    if (DumpStreamParticles.IsEmpty())
    {
        DumpStreamInstances->SetVisibility(false, true);
        DumpStreamInstances->SetHiddenInGame(true);
        return;
    }

    TArray<FTransform> Transforms;
    Transforms.Reserve(DumpStreamParticles.Num());
    for (const FDumpStreamParticle& Particle : DumpStreamParticles)
    {
        Transforms.Emplace(
            Particle.Rotation,
            Particle.Position,
            Particle.VisualScale
        );
    }
    DumpStreamInstances->AddInstances(
        Transforms,
        false,
        false,
        false
    );
    if (GranularParticleMaterial)
    {
        DumpStreamInstances->SetMaterial(
            0,
            GranularParticleMaterial
        );
    }
    else if (SoilMaterial)
    {
        DumpStreamInstances->SetMaterial(0, SoilMaterial);
    }
    DumpStreamInstances->SetVisibility(true, true);
    DumpStreamInstances->SetHiddenInGame(false);
}

void ADiggableTerrain::DumpBucketLoad(
    const FVector& LocalBucketTip,
    const FVector2D& LocalBucketForward,
    const float VolumeCubicCentimeters
)
{
    const float DumpedVolume = FMath::Min(
        FMath::Max(VolumeCubicCentimeters, 0.0f),
        CarriedSoilVolumeCubicCentimeters
    );
    SpawnDumpStream(
        LocalBucketTip,
        LocalBucketForward,
        DumpedVolume
    );
    CarriedSoilVolumeCubicCentimeters =
        FMath::Max(
            CarriedSoilVolumeCubicCentimeters - DumpedVolume,
            0.0f
    );
    UpdateBucketLoadVisual();
    bWasDumping = true;
    if (
        CarriedSoilVolumeCubicCentimeters
        <= BucketCapacityCubicMeters * 1000000.0f * 0.01f
    )
    {
        CarriedSoilVolumeCubicCentimeters = 0.0f;
        bLoggedFullBucket = false;
        bWasDumping = false;
        UpdateBucketLoadVisual();
        UE_LOG(LogDiggableTerrain, Log, TEXT("Bucket dump complete"));
    }
}
