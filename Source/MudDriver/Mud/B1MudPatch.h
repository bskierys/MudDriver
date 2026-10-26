#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "B1MudPatch.generated.h"

class UPhysicalMaterial;
class UStaticMeshComponent;
class AHUD;
class UCanvas;
class FDebugDisplayInfo;
class UTextureRenderTarget2D;
class UProceduralMeshComponent;

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

	/** Rut depth over UB1MudTrailSubsystem::TrailDepthRange, sampled by M_B1Mud as "TrailMask" (ADR-008); null outside game worlds */
	UTextureRenderTarget2D* GetTrailTarget() const { return TrailTarget; }

	/** UV on the trail target, the same as MudVisual's UV0. False outside the patch */
	bool WorldToTrailUV(const FVector& WorldLocation, FVector2D& OutUV) const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Collision only (hidden); the visible surface is MudVisual */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UStaticMeshComponent> MudSurface;

	/** Dense grid that M_B1Mud displaces into ruts (ADR-008); takes MudSurface's material. No collision */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UProceduralMeshComponent> MudVisual;

	/** Vertex spacing of MudVisual; ruts can't be sharper than this */
	UPROPERTY(EditAnywhere, Category = "Mud|Trails", meta = (Units = "cm", ClampMin = "2"))
	float VisualGridSpacing = 10.0f;

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

	void CreateTrailTarget();
	void BuildVisualMesh();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> TrailTarget;

	void UpdateEdgeRamp(UStaticMeshComponent& Ramp, const FVector2D& Outward, bool bEnabled);
};
