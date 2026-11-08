#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "B1MudLensComponent.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UB1MudSprayComponent;

/** Exposure of the active lens camera to one wheel's spray throw, with its factors */
struct FB1MudLensWheelExposure
{
	float Intensity = 0.0f;
	float Throw = 0.0f;
	float Aim = 0.0f;
	float Distance = 0.0f;
	float Exposure = 0.0f;
	/** cm/s */
	float LaunchSpeed = 0.0f;
	/** Horizontal angle between the throw and emitter → camera, deg */
	float AimAngle = 0.0f;
};

/** One splash on the lens: a random placement of the drop/speck fields, revealed by Strength */
struct FB1MudLensSlot
{
	float Strength = 0.0f;
	FVector2f Offset = FVector2f::ZeroVector;
	/** 0..7: rotation/mirror of the fields */
	int32 Variant = 0;
	float SinceHit = 0.0f;
};

/** Mud splashed on the chase camera's lens by the spray's throw (ADR-022). Shown by a post-process MID
 *  on the camera components tagged TargetTag; only the active one gathers mud */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1MudLensComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UB1MudLensComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Mud|Lens")
	FName TargetTag = TEXT("B1MudLens");

	/** 0..1 shows every slot at this strength without changing them; -1 = off */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (ClampMin = "-1", ClampMax = "1"))
	float PreviewLensDirt = -1.0f;

	/** Launch speed where the throw starts to reach the lens */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "cm/s", ClampMin = "0"))
	float LensMinThrowSpeed = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "cm/s", ClampMin = "0"))
	float LensFullThrowSpeed = 500.0f;

	/** Full aim while the throw is within this horizontal angle of emitter → camera */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "deg", ClampMin = "0", ClampMax = "180"))
	float LensAimFull = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "deg", ClampMin = "0", ClampMax = "180"))
	float LensAimZero = 110.0f;

	/** Emitter → camera distance where the distance factor is 0.5 */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "cm", ClampMin = "1"))
	float LensFalloffDistance = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (ClampMin = "0", ClampMax = "1"))
	float LensSplashThreshold = 0.1f;

	/** Mean time between splashes while exposed (±40 %) */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "s", ClampMin = "0.02"))
	float LensSplashInterval = 0.25f;

	/** Seconds at exposure 1 from a clean lens to LensDirt 1 */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "s", ClampMin = "0.1"))
	float LensSecondsToFull = 4.0f;

	/** A slot keeps its strength this long after its last hit */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "s", ClampMin = "0"))
	float LensHoldSeconds = 1.5f;

	/** Seconds for a slot to fade from 1 to 0 after the hold */
	UPROPERTY(EditAnywhere, Category = "Mud|Lens", meta = (Units = "s", ClampMin = "0.1"))
	float LensFadeSeconds = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Lens")
	int32 RandomSeed = 22;

	/** Mean slot strength; 1 = the concept image */
	float GetLensDirt() const;
	/** PreviewLensDirt when set, else GetLensDirt */
	float GetShownLensDirt() const;

	/** Max wheel exposure from the latest tick */
	float GetExposure() const { return Exposure; }
	const FB1MudLensWheelExposure& GetWheelExposure(int32 WheelIndex) const;
	int32 GetNumWheelExposures() const { return WheelExposures.Num(); }

	const FB1MudLensSlot& GetSlot(int32 SlotIndex) const { return Slots[SlotIndex]; }
	int32 GetSplashCount() const { return SplashCount; }
	int32 GetNumCameras() const { return Cameras.Num(); }

	void SetPreviewLensDirt(float Value);

	/** Debug: every slot to Value (0..1), held from now, preview off */
	void SetLensDirt(float Value);
	void CleanLens() { SetLensDirt(0.0f); }

	/** Debug: one splash as at exposure 1 */
	void Splash();

	static constexpr int32 NumSlots = 4;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void FindCameras();
	void AttachMaterial();
	void DetachMaterial();

	const UCameraComponent* GetActiveCamera() const;
	void UpdateExposure(const UCameraComponent* Camera);
	void UpdateSplashes(float DeltaTime);
	void AddSplash(float Amount);
	void FadeSlots(float DeltaTime);
	void PlaceSlot(FB1MudLensSlot& Slot);
	void WriteMaterial();
	void LogProgress();

	FB1MudLensSlot Slots[NumSlots];
	TArray<FB1MudLensWheelExposure> WheelExposures;
	float Exposure = 0.0f;
	float NextSplashIn = 0.0f;
	int32 SplashCount = 0;
	int32 LoggedTenth = 0;
	FRandomStream Random;

	/** Last values sent to the MID, per slot */
	FLinearColor WrittenSlots[NumSlots];
	bool bWrittenOnce = false;

	UPROPERTY(Transient)
	TObjectPtr<UB1MudSprayComponent> Spray;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCameraComponent>> Cameras;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
};
