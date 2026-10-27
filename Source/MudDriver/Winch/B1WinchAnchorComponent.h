#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "B1WinchAnchorComponent.generated.h"

/** Point a winch cable can hook onto (tree, post, log) */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1WinchAnchorComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	static UB1WinchAnchorComponent* FindNearest(const UWorld* World, const FVector& Location, float MaxDistance);
};
