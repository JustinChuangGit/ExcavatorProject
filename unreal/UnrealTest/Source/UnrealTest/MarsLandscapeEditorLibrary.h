#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MarsLandscapeEditorLibrary.generated.h"

/**
 * Small editor-only bridge used by the project setup scripts.
 *
 * Unreal's Landscape creation API is not exposed directly to Python. Keeping
 * the creation step here lets the Mars map own a normal ALandscape that can be
 * edited later with Unreal's standard Sculpt, Smooth, Flatten, and Ramp tools.
 */
UCLASS()
class UNREALTEST_API UMarsLandscapeEditorLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "Mars|Editor",
        meta = (DevelopmentOnly)
    )
    static AActor* CreateOrUpdateEditableMarsLandscape();
};
