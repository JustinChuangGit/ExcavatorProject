#include "MarsLandscapeEditorLibrary.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInterface.h"
#endif

namespace
{
constexpr TCHAR LandscapeActorLabel[] = TEXT("Mars_Editable_Landscape");
constexpr TCHAR MarsSoilMaterialPath[] =
    TEXT(
        "/Game/ExcavatorSim/Materials/"
        "M_Mars_RegolithVertex.M_Mars_RegolithVertex"
    );
}

AActor* UMarsLandscapeEditorLibrary::
    CreateOrUpdateEditableMarsLandscape()
{
#if WITH_EDITOR
    if (!GEditor)
    {
        return nullptr;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return nullptr;
    }

    UMaterialInterface* SoilMaterial =
        LoadObject<UMaterialInterface>(
            nullptr,
            MarsSoilMaterialPath
        );

    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        ALandscape* ExistingLandscape = *It;
        if (
            ExistingLandscape
            && ExistingLandscape->GetActorLabel()
                == LandscapeActorLabel
        )
        {
            ExistingLandscape->LandscapeMaterial = SoilMaterial;
            ExistingLandscape->MarkPackageDirty();
            ExistingLandscape->PostEditChange();
            return ExistingLandscape;
        }
    }

    // Two 63-quad components per axis create a 126 m square at Unreal's
    // standard 100 cm XY scale. This comfortably surrounds the 100 m
    // deformable regolith pad while remaining light enough to sculpt.
    constexpr int32 ComponentCount = 2;
    constexpr int32 NumSubsections = 1;
    constexpr int32 SubsectionSizeQuads = 63;
    constexpr int32 QuadsPerAxis =
        ComponentCount * NumSubsections * SubsectionSizeQuads;
    constexpr int32 VertexCountPerAxis = QuadsPerAxis + 1;
    constexpr float LandscapeScaleCentimeters = 100.0f;
    constexpr float LandscapeTopZ = -70.0f;

    const float HalfExtent =
        QuadsPerAxis * LandscapeScaleCentimeters * 0.5f;
    const FVector LandscapeOrigin(
        -HalfExtent,
        -HalfExtent,
        LandscapeTopZ
    );

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.OverrideLevel = World->GetCurrentLevel();
    ALandscape* Landscape = World->SpawnActor<ALandscape>(
        LandscapeOrigin,
        FRotator::ZeroRotator,
        SpawnParameters
    );
    if (!Landscape)
    {
        return nullptr;
    }

    Landscape->SetActorScale3D(
        FVector(
            LandscapeScaleCentimeters,
            LandscapeScaleCentimeters,
            100.0f
        )
    );
    Landscape->LandscapeMaterial = SoilMaterial;
    Landscape->SetActorLabel(LandscapeActorLabel);

    TArray<uint16> FlatHeightData;
    FlatHeightData.Init(
        32768,
        VertexCountPerAxis * VertexCountPerAxis
    );
    TMap<FGuid, TArray<uint16>> HeightDataPerLayer;
    HeightDataPerLayer.Add(FGuid(), MoveTemp(FlatHeightData));

    TMap<FGuid, TArray<FLandscapeImportLayerInfo>>
        MaterialLayerDataPerLayer;
    MaterialLayerDataPerLayer.Add(
        FGuid(),
        TArray<FLandscapeImportLayerInfo>()
    );

    Landscape->Import(
        FGuid::NewGuid(),
        0,
        0,
        QuadsPerAxis,
        QuadsPerAxis,
        NumSubsections,
        SubsectionSizeQuads,
        HeightDataPerLayer,
        nullptr,
        MaterialLayerDataPerLayer,
        ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>()
    );

    if (ULandscapeInfo* LandscapeInfo =
            Landscape->GetLandscapeInfo())
    {
        LandscapeInfo->UpdateLayerInfoMap(Landscape);
    }
    Landscape->RegisterAllComponents();
    Landscape->PostEditChange();
    Landscape->MarkPackageDirty();
    World->MarkPackageDirty();
    return Landscape;
#else
    return nullptr;
#endif
}
