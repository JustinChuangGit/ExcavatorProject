#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

#include "DiggableTerrain.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UExcavatorVendorAdapterComponent;
class UExcavatorROSBridgeComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class ACameraActor;
class AExcavatorOperatorCharacter;
class APawn;

/**
 * Lightweight height-field soil for the Mars excavation proof of concept.
 *
 * The mesh owns its render and collision geometry. It finds the excavator's
 * bucket bone at runtime, removes soil under a moving bucket tip, carries the
 * captured volume in a bucket-shaped procedural mesh, and deposits displaced
 * material into nearby spoil.
 */
UCLASS(Blueprintable)
class UNREALTEST_API ADiggableTerrain final : public AActor
{
    GENERATED_BODY()

public:
    ADiggableTerrain();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil"
    )
    TObjectPtr<UProceduralMeshComponent> TerrainMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil"
    )
    TObjectPtr<UProceduralMeshComponent> StableCollisionMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil"
    )
    TObjectPtr<UStaticMeshComponent> BucketLoadMesh;

    /**
     * Runtime-generated regolith volume fitted to the bucket interior.
     * BucketLoadMesh remains above only for compatibility with placed actors
     * made before the procedural load was introduced.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil"
    )
    TObjectPtr<UProceduralMeshComponent> BucketSoilMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil"
    )
    TObjectPtr<UInstancedStaticMeshComponent> GranularSoilInstances;

    /**
     * A small pool of short-lived visual clumps used only while soil is
     * falling from the bucket. The deformable height field remains the
     * authoritative soil mass.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil"
    )
    TObjectPtr<UInstancedStaticMeshComponent> DumpStreamInstances;

    /**
     * Sleeping clumps spread across the work site. They make the broad terrain
     * read as regolith without putting tens of thousands of distant pieces
     * through the contact solver.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil"
    )
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
        SurfaceClumpInstances;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype"
    )
    TObjectPtr<UMaterialInterface> GranularParticleMaterialBase;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Geometry",
        meta = (ClampMin = "64", ClampMax = "768")
    )
    int32 GridResolution = 96;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Geometry",
        meta = (ClampMin = "8.0", ClampMax = "250.0")
    )
    float CellSizeCentimeters = 75.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Geometry",
        meta = (ClampMin = "16", ClampMax = "128")
    )
    int32 TileCellCount = 64;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Geometry",
        meta = (ClampMin = "8.0", ClampMax = "30.0")
    )
    float RuntimeTargetCellSizeCentimeters = 12.5f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Geometry",
        meta = (ClampMin = "0.0", ClampMax = "200.0")
    )
    float TerrainReliefCentimeters = 65.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Appearance"
    )
    TObjectPtr<UMaterialInterface> SoilMaterial;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Camera"
    )
    bool bSetInitialCameraView = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Camera"
    )
    FRotator InitialCameraControlRotation =
        FRotator(-18.0f, 0.0f, 0.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction"
    )
    FName BucketBoneName =
        TEXT("B_ConstractionExcavator01_End");

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "10.0", ClampMax = "400.0")
    )
    float DigRadiusCentimeters = 26.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "1.0", ClampMax = "30.0")
    )
    float MaximumCutPerStepCentimeters = 12.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "25.0", ClampMax = "500.0")
    )
    float MaximumDigDepthCentimeters = 220.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "0.02", ClampMax = "0.5")
    )
    float InteractionIntervalSeconds = 0.05f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "5.0", ClampMax = "150.0")
    )
    float ContactToleranceCentimeters = 28.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "0.05", ClampMax = "5.0")
    )
    float MinimumTipMotionCentimeters = 0.35f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "0.0", ClampMax = "150.0")
    )
    float BucketProbeSpreadCentimeters = 16.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load"
    )
    FVector BucketSoilRelativeLocation =
        FVector(4.0f, 0.0f, 80.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load"
    )
    FRotator BucketSoilRelativeRotation =
        FRotator(180.0f, 0.0f, 0.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "40.0", ClampMax = "180.0")
    )
    float BucketInteriorLengthCentimeters = 52.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "30.0", ClampMax = "160.0")
    )
    float BucketInteriorWidthCentimeters = 46.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "20.0", ClampMax = "100.0")
    )
    float BucketInteriorDepthCentimeters = 34.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "0.0", ClampMax = "40.0")
    )
    float BucketHeapHeightCentimeters = 15.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "0.05", ClampMax = "2.0")
    )
    float BucketCapacityCubicMeters = 0.05f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float PassiveCaptureEfficiency = 0.16f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float CurlCaptureEfficiency = 0.90f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "0.1", ClampMax = "2.0")
    )
    float MaximumBucketFillFractionPerSecond = 0.45f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "0.05", ClampMax = "2.0")
    )
    float DumpRateCubicMetersPerSecond = 0.10f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Bucket Load",
        meta = (ClampMin = "0.0", ClampMax = "0.8")
    )
    float DumpTiltThreshold = 0.12f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype"
    )
    bool bEnableGranularSoil = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "2.0", ClampMax = "10.0")
    )
    float GranularParticleRadiusCentimeters = 3.2f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "150.0", ClampMax = "1000.0")
    )
    float GranularPatchLengthCentimeters = 560.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "150.0", ClampMax = "800.0")
    )
    float GranularPatchWidthCentimeters = 400.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "10.0", ClampMax = "100.0")
    )
    float GranularBedDepthCentimeters = 14.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "500", ClampMax = "30000")
    )
    int32 MaximumGranularParticles = 10000;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "1", ClampMax = "6")
    )
    int32 GranularSolverIterations = 2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float GranularContactStiffness = 0.72f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float GranularSurfaceFriction = 0.82f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float GranularCohesionStrength = 0.34f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "1.0", ClampMax = "1.6")
    )
    float GranularCohesionRangeMultiplier = 1.30f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "50.0", ClampMax = "500.0")
    )
    float GranularActivationRadiusCentimeters = 175.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Surface Clumps"
    )
    bool bEnableSurfaceClumps = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Surface Clumps",
        meta = (ClampMin = "500.0", ClampMax = "3400.0")
    )
    float SurfaceClumpHalfExtentCentimeters = 3000.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Surface Clumps",
        meta = (ClampMin = "18.0", ClampMax = "80.0")
    )
    float SurfaceClumpSpacingCentimeters = 58.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Granular Prototype",
        meta = (ClampMin = "100.0", ClampMax = "980.0")
    )
    float GranularGravityCentimetersPerSecondSquared = 371.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Mars Soil|Granular Prototype"
    )
    int32 ActiveGranularParticleCount = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "10.0", ClampMax = "150.0")
    )
    float MaximumBucketPenetrationCentimeters = 28.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Interaction",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float SpoilRetentionRatio = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Collision",
        meta = (ClampMin = "0.05", ClampMax = "1.0")
    )
    float CollisionUpdateIntervalSeconds = 0.20f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mars Soil|Collision",
        meta = (ClampMin = "100.0", ClampMax = "800.0")
    )
    float CollisionSafetyRadiusCentimeters = 330.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Operator",
        meta = (ClampMin = "150.0", ClampMax = "1000.0")
    )
    float ExcavatorEnterDistanceCentimeters = 500.0f;

    UFUNCTION(BlueprintCallable, Category = "Mars Soil")
    void ResetTerrain();

private:
    struct FGranularParticle
    {
        FVector Position = FVector::ZeroVector;
        FVector PreviousPosition = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;
        FQuat VisualRotation = FQuat::Identity;
        FVector VisualScale = FVector::OneVector;
        FVector CarriedLocalPosition = FVector::ZeroVector;
        int32 CohesionCluster = INDEX_NONE;
        float SleepSeconds = 0.0f;
        bool bActive = false;
        bool bGrounded = false;
        bool bCarried = false;
    };

    struct FDumpStreamParticle
    {
        FVector Position = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;
        FQuat Rotation = FQuat::Identity;
        FVector VisualScale = FVector::OneVector;
        float VolumeCubicCentimeters = 0.0f;
        float AgeSeconds = 0.0f;
    };

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;
    TArray<float> InitialHeights;
    TArray<float> CurrentHeights;
    TSet<int32> DirtyTerrainTiles;
    TSet<int32> PendingCollisionTiles;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UProceduralMeshComponent>> CollisionTileMeshes;

    TWeakObjectPtr<USkeletalMeshComponent> ExcavatorMesh;
    TWeakObjectPtr<UExcavatorVendorAdapterComponent> ExcavatorAdapter;
    TWeakObjectPtr<UExcavatorROSBridgeComponent> ExcavatorROSBridge;
    TWeakObjectPtr<ACameraActor> DebugBucketCamera;
    TWeakObjectPtr<AExcavatorOperatorCharacter> OperatorCharacter;
    TWeakObjectPtr<APawn> ExcavatorPawn;
    FVector LastBucketTipWorld = FVector::ZeroVector;
    FVector2D LastDigLocation = FVector2D::ZeroVector;
    FRotator SavedExcavatorControlRotation = FRotator::ZeroRotator;
    float InteractionAccumulator = 0.0f;
    float CarriedSoilVolumeCubicCentimeters = 0.0f;
    float CollisionUpdateAccumulator = 0.0f;
    bool bHasPreviousBucketTip = false;
    bool bDebugBucketFill = false;
    bool bWasDigging = false;
    bool bWasDumping = false;
    bool bResetButtonWasDown = false;
    bool bOperatorToggleWasDown = false;
    bool bOperatorOnFoot = false;
    bool bStartOperatorOnFootRequested = false;
    bool bLoggedInitialBucketTip = false;
    bool bLoggedFirstDig = false;
    bool bLoggedFullBucket = false;
    uint32 ObservedResetGeneration = 0;
    uint32 ObservedOperatorToggleGeneration = 0;

    TArray<FGranularParticle> GranularParticles;
    TArray<int32> GranularNextInCell;
    TMap<FIntVector, int32> GranularCellHeads;
    TArray<FTransform> GranularInstanceTransforms;
    FVector2D GranularPatchCenterLocal = FVector2D::ZeroVector;
    FTransform PreviousBucketCavityTransform = FTransform::Identity;
    FTransform LastGranularActivationTransform = FTransform::Identity;
    float GranularSimulationAccumulator = 0.0f;
    float GranularRenderAccumulator = 0.0f;
    double GranularBenchmarkSeconds = 0.0;
    int32 GranularBenchmarkSteps = 0;
    bool bGranularInitialized = false;
    bool bHasPreviousBucketCavityTransform = false;
    bool bHasGranularActivationTransform = false;
    bool bSurfaceClumpsInitialized = false;
    bool bGranularTransformsDirty = false;
    TArray<FDumpStreamParticle> DumpStreamParticles;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GranularParticleMaterial;

    void GenerateTerrain();
    void BuildTopology();
    void RebuildDerivedMeshData();
    void UpdateTerrainMesh();
    void BuildTerrainTileData(
        int32 TileX,
        int32 TileY,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector>& OutNormals,
        TArray<FVector2D>& OutUVs,
        TArray<FLinearColor>& OutVertexColors,
        TArray<FProcMeshTangent>& OutTangents
    ) const;
    int32 TerrainTileCountPerAxis() const;
    int32 TerrainTileIndexForVertex(int32 X, int32 Y) const;
    void MarkTerrainVertexDirty(int32 X, int32 Y);
    void MarkAllTerrainTilesDirty();
    void InitializeCollisionTiles();
    void UpdatePendingCollisionTiles(float DeltaTime);
    void ResolveExcavatorMesh();
    void ProcessBucketInteraction();
    bool DigAt(
        const FVector& LocalBucketTip,
        const FVector2D& LocalMotion,
        const FVector2D& LocalBucketAcross
    );
    void DepositSpoil(
        const FVector2D& Center,
        float VolumeCubicCentimeters
    );
    void RelaxSpoilPile(const FVector2D& Center, float Radius);
    float SampleHeight(const FVector2D& LocalPosition) const;
    int32 VertexIndex(int32 X, int32 Y) const;
    FVector2D GridMinimum() const;
    bool IsInsideTerrain(const FVector2D& LocalPosition) const;
    void UpdateOperatorModeInput();
    void ExitExcavator();
    void TryEnterExcavator(bool bIgnoreDistance = false);
    bool FindOperatorExitTransform(FTransform& OutTransform) const;
    void UpdateResetInput();
    void UpdateRosReset();
    void UpdateBucketLoadVisual();
    void BuildBucketLoadMesh(float FillRatio);
    void InitializeGranularSoil();
    void InitializeSurfaceClumps();
    void ResetGranularSoil();
    void TickGranularSoil(float DeltaTime);
    void StepGranularSoil(float FixedDeltaTime);
    void BuildGranularSpatialHash();
    void SolveGranularParticleContacts();
    bool SolveGranularBucketContact(
        FGranularParticle& Particle,
        const FTransform& BucketCavityTransform,
        const FTransform& PreviousCavityTransform,
        float FixedDeltaTime
    );
    void SolveGranularTerrainContact(
        FGranularParticle& Particle
    );
    void UpdateGranularInstanceTransforms();
    void TickDumpStream(float DeltaTime);
    void SpawnDumpStream(
        const FVector& LocalBucketTip,
        const FVector2D& LocalBucketForward,
        float VolumeCubicCentimeters
    );
    void UpdateDumpStreamInstances();
    FIntVector GranularCellForPosition(
        const FVector& Position
    ) const;
    void DumpBucketLoad(
        const FVector& LocalBucketTip,
        const FVector2D& LocalBucketForward,
        float VolumeCubicCentimeters
    );
};
