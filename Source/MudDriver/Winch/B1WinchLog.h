#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "B1WinchLog.generated.h"

class AHUD;
class FDebugDisplayInfo;
class UBoxComponent;
class UCanvas;
class UStaticMeshComponent;
class UB1WinchAnchorComponent;

UENUM()
enum class EB1LogState : uint8
{
	/** Kinematic: a wall to the jeep */
	Frozen,
	/** Simulating */
	Free,
};

/**
 * Fallen tree (ADR-018): frozen until the hooked cable's tension reaches BreakawayTension, then moved by Chaos alone.
 * Refreezes once at rest. Root is an unscaled box (flat sides, no roll); the round trunk is visual only.
 */
UCLASS()
class MUDDRIVER_API AB1WinchLog : public AActor
{
	GENERATED_BODY()

public:

	AB1WinchLog();

	virtual void Tick(float DeltaSeconds) override;

	/** Along the body's X; the root end is at -X */
	UPROPERTY(EditAnywhere, Category = "Log|Shape", meta = (Units = "cm", ClampMin = "100"))
	float TrunkLength = 1000.0f;

	/** Half the box's height and width; taller than the jeep can climb */
	UPROPERTY(EditAnywhere, Category = "Log|Shape", meta = (Units = "cm", ClampMin = "5"))
	float TrunkRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Log|Shape", meta = (Units = "cm", ClampMin = "5"))
	float RootBallRadius = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Log|Config", meta = (Units = "kg", ClampMin = "1"))
	float TrunkMass = 1800.0f;

	/** Welded at the root end, so the compound COM sits toward the root */
	UPROPERTY(EditAnywhere, Category = "Log|Config", meta = (Units = "kg", ClampMin = "1"))
	float RootBallMass = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Log|Config", meta = (Units = "N", ClampMin = "0"))
	float BreakawayTension = 40000.0f;

	/** Below breakaway and at rest this long before freezing again */
	UPROPERTY(EditAnywhere, Category = "Log|Config", meta = (Units = "s", ClampMin = "0"))
	float RefreezeDelay = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Log|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float RefreezeLinearSpeed = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Log|Config", meta = (Units = "deg/s", ClampMin = "0"))
	float RefreezeAngularSpeed = 2.0f;

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Pushes TrunkMass / RootBallMass to the bodies */
	void ApplyMass();

	/** Sizes the box, the trunk visual, the root ball and the anchor area from Log|Shape */
	void ApplyShape();

	/** Back to the placed pose, frozen, switch count cleared (cheat B1LogReset) */
	void ResetToPlacedPose();

	EB1LogState GetState() const { return State; }
	int32 GetSwitchCount() const { return SwitchCount; }
	/** Published tension of the winch hooked here, N */
	float GetTension() const;
	float GetYawSincePlaced() const;
	float GetMass() const;
	const FTransform& GetPlacedTransform() const { return PlacedTransform; }
	UB1WinchAnchorComponent* GetAnchor() const { return Anchor; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log|Components")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log|Components")
	TObjectPtr<UStaticMeshComponent> TrunkVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log|Components")
	TObjectPtr<UStaticMeshComponent> RootBall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log|Components")
	TObjectPtr<UB1WinchAnchorComponent> Anchor;

private:

	void SetState(EB1LogState NewState, float Tension);
	void DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

	FTransform PlacedTransform;
	EB1LogState State = EB1LogState::Frozen;
	int32 SwitchCount = 0;
	float RestTime = 0.0f;
	FDelegateHandle ShowDebugHandle;
};
