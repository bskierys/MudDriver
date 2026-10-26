#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "B1MudTrailSubsystem.generated.h"

class AB1MudPatch;
class UTexture2D;

/**
 * Visual mud ruts (ADR-008): wheels add segments, the subsystem splits them into soft brush stamps of rut depth
 * and draws them into each patch's trail render target once per frame. No effect on physics.
 */
UCLASS()
class MUDDRIVER_API UB1MudTrailSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterPatch(AB1MudPatch* Patch);
	void UnregisterPatch(AB1MudPatch* Patch);

	/** Stamps every StampSpacing along the segment, on every patch it crosses; the rut there blends toward DepthCm */
	void AddSegment(const FVector& From, const FVector& To, float WidthCm, float DepthCm);

	/** cm between stamps along a wheel's path */
	static constexpr float StampSpacing = 5.0f;

	/** Rut depth stored as 1 in the trail target (cm); deeper than the deepest mud preset */
	static constexpr float TrailDepthRange = 100.0f;

private:

	struct FStamp
	{
		FVector2D UV;
		float WidthCm = 0.0f;
		float DepthCm = 0.0f;
	};

	void AddStamp(const FVector& WorldLocation, float WidthCm, float DepthCm);
	void CreateBrushTexture();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AB1MudPatch>> Patches;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BrushTexture;

	TMap<TWeakObjectPtr<AB1MudPatch>, TArray<FStamp>> PendingStamps;
};
