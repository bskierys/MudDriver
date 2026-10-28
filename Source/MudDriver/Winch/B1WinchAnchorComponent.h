#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "B1Interactable.h"
#include "B1WinchAnchorComponent.generated.h"

/** Point a winch cable can hook onto (tree, post, log) */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1WinchAnchorComponent : public USceneComponent, public IB1Interactable
{
	GENERATED_BODY()

public:

	static UB1WinchAnchorComponent* FindNearest(const UWorld* World, const FVector& Location, float MaxDistance);

	/** "Attach" while the character carries a hook in reach, "Detach" while a winch is hooked here */
	virtual void GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const override;

	/** Attach parent when it simulates physics (log): the winch gives it the opposite cable force */
	UPrimitiveComponent* GetSimulatingBody() const;

protected:

	/** Unhooks winches first, so the sim never reads a destroyed body */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
