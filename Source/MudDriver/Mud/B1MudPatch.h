#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "B1MudPatch.generated.h"

class UPhysicalMaterial;
class UStaticMeshComponent;

/**
 * Mud patch per ADR-012: a wheel-trace-only surface slab with its top at the actor origin,
 * and an optional hard bottom that everything collides with.
 */
UCLASS()
class MUDDRIVER_API AB1MudPatch : public AActor
{
	GENERATED_BODY()

public:

	AB1MudPatch();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UStaticMeshComponent> MudSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UStaticMeshComponent> HardBottom;

	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "cm", ClampMin = "10"))
	FVector2D Size = FVector2D(2000.0f, 2000.0f);

	/** Reported to the wheels as the contact material */
	UPROPERTY(EditAnywhere, Category = "Mud|Config")
	TObjectPtr<UPhysicalMaterial> MudMaterial;

	/** Off when the landscape or another mesh is the bottom */
	UPROPERTY(EditAnywhere, Category = "Mud|Config")
	bool bHardBottom = true;

	/** Hard bottom top, below the mud surface */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "cm", ClampMin = "0", EditCondition = "bHardBottom"))
	float HardBottomDepth = 50.0f;
};
