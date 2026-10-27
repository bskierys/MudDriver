#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "B1MudTrailSurfaceComponent.generated.h"

class UPrimitiveComponent;
class UTextureRenderTarget2D;

/**
 * Receives visual ruts (ADR-008) for one mud surface: a trail render target over a local XY rectangle of the surface
 * primitive, centred on it, put on the primitive's material as "TrailMask". Optional per ADR-015.
 */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1MudTrailSurfaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Game worlds only. Surface's UV0 must span the rectangle the way WorldToTrailUV maps it */
	void Initialize(UPrimitiveComponent* InSurface, const FVector2D& InSize);

	/** Rut depth over UB1MudTrailSubsystem::TrailDepthRange; null before Initialize */
	UTextureRenderTarget2D* GetTrailTarget() const { return TrailTarget; }

	/** cm, local XY */
	const FVector2D& GetSize() const { return Size; }

	/** UV = local XY / Size + 0.5. False outside the rectangle */
	bool WorldToTrailUV(const FVector& WorldLocation, FVector2D& OutUV) const;

	/** World box the stamps can land in; a cheap prefilter before WorldToTrailUV */
	const FBox& GetWorldBounds() const { return WorldBounds; }

	/** Texels per side */
	UPROPERTY(EditAnywhere, Category = "Mud|Trails", meta = (ClampMin = "16"))
	int32 Resolution = 512;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> Surface;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> TrailTarget;

	FVector2D Size = FVector2D::ZeroVector;
	FBox WorldBounds = FBox(ForceInit);
};
