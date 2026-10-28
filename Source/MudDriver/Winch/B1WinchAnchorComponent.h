#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "B1Interactable.h"
#include "B1WinchAnchorComponent.generated.h"

/**
 * Area a winch cable can hook onto (tree, post, log): a segment on the trunk axis plus a radius.
 * Area points are in the component's frame without scale, so a scaled parent doesn't stretch them.
 */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1WinchAnchorComponent : public USceneComponent, public IB1Interactable
{
	GENERATED_BODY()

public:

	/** Both at the origin = a single hook point at the component */
	UPROPERTY(EditAnywhere, Category = "Winch|Anchor")
	FVector AreaStart = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Winch|Anchor")
	FVector AreaEnd = FVector::ZeroVector;

	/** The strap wraps the trunk at this radius; the cable leaves its surface */
	UPROPERTY(EditAnywhere, Category = "Winch|Anchor", meta = (Units = "cm", ClampMin = "0"))
	float TrunkRadius = 0.0f;

	/** Nearest area point within MaxDistance */
	static UB1WinchAnchorComponent* FindNearest(const UWorld* World, const FVector& Location, float MaxDistance);

	/** "Attach" while the character carries a hook in reach, "Detach" while a winch is hooked here */
	virtual void GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const override;

	/** Attach parent when it simulates physics (log): the winch gives it the opposite cable force */
	UPrimitiveComponent* GetSimulatingBody() const;

	/** World point on the trunk axis, Alpha 0 = AreaStart, 1 = AreaEnd */
	FVector GetAreaPoint(float Alpha) const;
	float GetAreaAlphaClosestTo(const FVector& WorldPoint) const;
	/** Alpha of the area point closest to the ray segment Origin + Direction * [0, Length] */
	float GetAreaAlphaClosestToRay(const FVector& Origin, const FVector& Direction, float Length) const;
	FVector GetClosestAreaPoint(const FVector& WorldPoint) const { return GetAreaPoint(GetAreaAlphaClosestTo(WorldPoint)); }

	/** World unit vector along the trunk; the component's up for a single point */
	FVector GetAreaAxis() const;
	/** Point on a ring of TrunkRadius + Clearance around the axis at Alpha, on the side facing Toward */
	FVector GetSurfacePoint(float Alpha, const FVector& Toward, float Clearance) const;
	/** World transform of a unit (100 cm) Z cylinder as a ring of TrunkRadius + Clearance, Width cm wide, at Alpha */
	FTransform GetRingTransform(float Alpha, float Clearance, float Width) const;

protected:

	/** Unhooks winches first, so the sim never reads a destroyed body */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	FVector ToWorld(const FVector& Local) const;
};
