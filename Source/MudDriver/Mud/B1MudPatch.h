#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "B1MudPatch.generated.h"

class UPhysicalMaterial;
class UStaticMeshComponent;
class AHUD;
class UCanvas;
class FDebugDisplayInfo;

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

	UPhysicalMaterial* GetMudMaterial() const { return MudMaterial; }
	const FVector2D& GetSize() const { return Size; }
	bool HasHardBottom() const { return bHardBottom; }
	float GetHardBottomDepth() const { return HardBottomDepth; }
	float GetEdgeRampLength() const { return EdgeRampLength; }

	/** e.g. "+X -X", or "none" */
	FString DescribeRamps() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UStaticMeshComponent> MudSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UStaticMeshComponent> HardBottom;

	/** +X, -X, +Y, -Y */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TArray<TObjectPtr<UStaticMeshComponent>> EdgeRamps;

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

	/** The hard bottom climbs back to the surface at these edges. Leave an edge open to butt another patch against it */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (EditCondition = "bHardBottom"))
	bool bRampPlusX = false;

	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (EditCondition = "bHardBottom"))
	bool bRampMinusX = false;

	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (EditCondition = "bHardBottom"))
	bool bRampPlusY = false;

	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (EditCondition = "bHardBottom"))
	bool bRampMinusY = false;

	/** Horizontal length of an edge ramp, from the bottom to the surface */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "cm", ClampMin = "10", EditCondition = "bHardBottom"))
	float EdgeRampLength = 300.0f;

private:

	/** `ShowDebug B1Mud`, only within B1MudShowDebug::PatchDrawDistance of the viewer */
	void DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);
	FDelegateHandle ShowDebugHandle;

	void UpdateEdgeRamp(UStaticMeshComponent& Ramp, const FVector2D& Outward, bool bEnabled);
};
