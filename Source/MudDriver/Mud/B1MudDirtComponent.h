#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "B1MudDirtComponent.generated.h"

class UPrimitiveComponent;
class UB1MudVehicleComponent;

/** Mud caked on the vehicle, sent to MF_B1MudDirt as Custom Primitive Data on components tagged TargetTag (ADR-021).
 *  CPD: body slot 0 = BodyDirt, slots 1..4 = WheelDirt[0..3]; tire slot 0 = its wheel's dirt, 1 = coat, 2 = wetness. Grows from B1MudThrow intensity */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1MudDirtComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UB1MudDirtComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** A tagged component near a wheel (within its radius) is that wheel's tire; any other is body */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt")
	FName TargetTag = TEXT("B1MudDirt");

	/** 0..1 overrides every dirt value without changing them, in the editor and in game; -1 = off */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt", meta = (ClampMin = "-1", ClampMax = "1"))
	float PreviewDirt = -1.0f;

	/** Seconds at full B1MudThrow intensity from clean to 1 */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt", meta = (Units = "s", ClampMin = "0.1"))
	float WheelSecondsToFull = 15.0f;

	/** Seconds at full mean wheel intensity from clean to 1 */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt", meta = (Units = "s", ClampMin = "0.1"))
	float BodySecondsToFull = 45.0f;

	/** Belly depth (past clearance) where the belly term reaches BellyBodyRate */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt", meta = (Units = "cm", ClampMin = "1"))
	float BellyFullDepth = 20.0f;

	/** Extra body rate from a sunk belly while the wheels move, as a multiple of the full wheel term */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt", meta = (ClampMin = "0"))
	float BellyBodyRate = 1.0f;

	/** Tire coat after one wheel rotation (tread distance 2πr) on mud: the tread band is covered */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt|Tire", meta = (ClampMin = "0", ClampMax = "1"))
	float TreadCoat = 0.4f;

	/** Seconds on mud from TreadCoat to a full coat (reaching the rim) */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt|Tire", meta = (Units = "s", ClampMin = "0.1"))
	float TireCoatSecondsToFull = 20.0f;

	/** Tread distance off mud that sheds a full coat */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt|Tire", meta = (Units = "m", ClampMin = "1"))
	float TireShedDistance = 150.0f;

	/** Seconds off mud from wet (1) to dry */
	UPROPERTY(EditAnywhere, Category = "Mud|Dirt|Tire", meta = (Units = "s", ClampMin = "0.1"))
	float TireDrySeconds = 40.0f;

	void SetPreviewDirt(float Value);

	/** Debug: every accumulated value (tire coat and wetness too) to Value (0..1) and preview off; may lower dirt */
	void SetDirt(float Value);

	/** Fast tire layer on top of the crust; unlike dirt it goes down off mud */
	float GetTireCoat(int32 WheelIndex) const { return TireCoat.IsValidIndex(WheelIndex) ? TireCoat[WheelIndex] : 0.0f; }
	float GetTireWetness(int32 WheelIndex) const { return TireWetness.IsValidIndex(WheelIndex) ? TireWetness[WheelIndex] : 0.0f; }

	/** Debug: SetDirt(0) */
	void Clean() { SetDirt(0.0f); }

	float GetWheelDirt(int32 WheelIndex) const { return WheelDirt.IsValidIndex(WheelIndex) ? WheelDirt[WheelIndex] : 0.0f; }
	float GetBodyDirt() const { return BodyDirt; }

	/** B1MudThrow intensity from the latest tick */
	float GetWheelIntensity(int32 WheelIndex) const { return WheelIntensity.IsValidIndex(WheelIndex) ? WheelIntensity[WheelIndex] : 0.0f; }

	/** Values the material shows: PreviewDirt when set, else the accumulated ones */
	float GetShownWheelDirt(int32 WheelIndex) const;
	float GetShownBodyDirt() const;

	/** Tagged components found in game; 0 before the wheels exist */
	int32 GetNumTireTargets() const;
	int32 GetNumBodyTargets() const;

	static constexpr int32 NumWheelSlots = 4;

protected:

	virtual void OnRegister() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	struct FTarget
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		/** INDEX_NONE = body */
		int32 Wheel = INDEX_NONE;
	};

	/** Needs the wheels, which come with the physics state, so it may run a few ticks after BeginPlay */
	bool ResolveTargets();

	/** Before targets are resolved (editor), every tagged part gets PreviewDirt (or 0 when off) in every slot */
	void ApplyPreviewToTagged();

	/** Dirt only grows here; reads the mud component, never the sim or the spray */
	void Accumulate(float DeltaTime);

	void UpdateTireLayer(int32 WheelIndex, bool bOnMud, float DeltaTime);

	/** One log line each time the highest value passes a new tenth */
	void LogProgress();
	int32 LoggedTenth = 0;

	void WriteTargets();

	/** Only writes when the component's value differs by MinCPDChange or more, so re-registered parts are caught too */
	static void WriteSlot(UPrimitiveComponent& Component, int32 Slot, float Value);

	TArray<float> WheelDirt;
	TArray<float> WheelIntensity;
	TArray<float> TireCoat;
	TArray<float> TireWetness;
	float BodyDirt = 0.0f;

	TArray<FTarget> Targets;
	bool bTargetsResolved = false;

	UPROPERTY(Transient)
	TObjectPtr<UB1MudVehicleComponent> MudVehicle;
};
