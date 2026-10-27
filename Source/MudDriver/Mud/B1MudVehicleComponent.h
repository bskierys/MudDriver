#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "B1MudSimTypes.h"
#include "B1FuelLoadSource.h"
#include "B1MudVehicleComponent.generated.h"

class IB1MudSimHost;
class FPhysScene_Chaos;
class UB1PhysicalMaterial_Mud;
class APawn;
class AController;
struct FWheelStatus;

/** Mud on a wheeled vehicle: mud params to the physics sim, its results back, the InMud tag and B1Unstuck. Needs an IB1MudSimHost on the owner */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1MudVehicleComponent : public UActorComponent, public IB1FuelLoadSource
{
	GENERATED_BODY()

public:

	UB1MudVehicleComponent();

	int32 GetNumWheels() const;
	const FWheelStatus& GetWheelState(int32 WheelIndex) const;
	float GetWheelRadius(int32 WheelIndex) const;
	float GetWheelWidth(int32 WheelIndex) const;

	/** The wheel's mud material; null when airborne or off mud */
	const UB1PhysicalMaterial_Mud* GetWheelMud(int32 WheelIndex) const;

	/** cm, from the latest physics step; 0 off mud */
	float GetWheelSinkDepth(int32 WheelIndex) const;

	/** Mud friction curve value from the latest physics step; 1 off mud */
	float GetWheelMudFrictionScale(int32 WheelIndex) const;

	/** N, from the latest physics step */
	float GetWheelRollingResistance(int32 WheelIndex) const;

	/** N pulling the wheel down a muddy slope, from the latest physics step */
	float GetWheelSlopePull(int32 WheelIndex) const;

	/** cm the average sink depth is past BellyClearance */
	float GetBellyDepth() const { return SimOutput.BellyDepth; }

	/** N */
	float GetBellyDrag() const { return SimOutput.BellyDrag; }

	/** cm, mean over all wheels */
	float GetAverageSinkDepth() const;

	/** B1Unstuck: teleports the vehicle level, wheels 50 cm above the mud under it, stops it and clears every wheel's sink.
	 *  Then for UnstuckAssistDuration, throttle/brake push the vehicle along its heading */
	void Unstuck();

	bool IsUnstuckAssistActive() const { return UnstuckAssistTimeLeft > 0.0f; }

	/** Belly drag starts once the average sink depth passes it. Axle height, below the jeep body's ~62 cm underside */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "cm", ClampMin = "0"))
	float BellyClearance = 45.0f;

	/** Mud steeper than this is a slope: the vehicle stays awake, with no input its wheels aren't braked, and SlopePull applies */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "deg", ClampMin = "0", ClampMax = "45"))
	float KeepAwakeSlope = 3.0f;

	/** Slippery mud: extra pull down the slope per mud wheel, as a fraction of gravity along the ground (ADR-001) */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (ClampMin = "0"))
	float SlopePull = 0.5f;

	/** Push at full throttle during the B1Unstuck window, along the flattened heading */
	UPROPERTY(EditAnywhere, Category = "Mud|Unstuck", meta = (Units = "N", ClampMin = "0"))
	float UnstuckAssistForce = 15000.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Unstuck", meta = (Units = "s", ClampMin = "0"))
	float UnstuckAssistDuration = 4.0f;

	/** InMudTag is on the owner's ASC: a wheel touches mud, or did within MudTagReleaseDelay */
	bool IsInMud() const { return bInMud; }

	/** Effective throttle × average sink depth over MudFullStrainDepth, [0, 1]. A fuel load for the engine fighting mud (ADR-010) */
	float GetMudStrain() const;

	/** Loose tag on the owner's ASC while in mud; none set means no tag */
	UPROPERTY(EditAnywhere, Category = "Mud|Config")
	FGameplayTag InMudTag;

	/** Keeps the tag through bumps where wheels briefly leave the mud */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "s", ClampMin = "0"))
	float MudTagReleaseDelay = 0.5f;

	/** Average wheel sink depth where mud strain reaches 1; strain scales linearly below it */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "cm", ClampMin = "1"))
	float MudFullStrainDepth = 50.0f;

	/** Mud strain is reported to fuel consumption under this name (UB1FuelConsumptionData::LoadRates) */
	UPROPERTY(EditAnywhere, Category = "Mud|Config")
	FName FuelLoadName = TEXT("Mud");

	virtual void AddFuelLoads(TArray<FB1FuelLoad>& OutLoads) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Only while the owner pawn is controlled and has an ASC */
	void UpdateInMud(float DeltaTime);
	void SetInMud(bool bNewInMud);

	/** Possession changed: the tag belongs to the old driver's ASC, so it's dropped (ADR-004) */
	UFUNCTION()
	void OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

private:

	/** Pushes input to the physics sim and reads its last result; no component tick */
	void OnPhysSceneStep(FPhysScene_Chaos* Scene, float DeltaTime);
	FDelegateHandle PhysSceneStepHandle;

	/** Adds the mud materials the wheels touch now; params of every known one go to the sim */
	void GatherMudParams(TArray<FB1MudSimParams>& OutParams);

	FVector GetUnstuckAssistForce(float DeltaTime);

	/** Logs the chassis underside height over firm ground once, to check BellyClearance */
	void LogMeasuredClearance();
	bool bLoggedClearance = false;

	IB1MudSimHost* Host = nullptr;

	/** Grows as wheels touch new mud. The sim treats a material as firm until it's in here, about a frame late */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UB1PhysicalMaterial_Mud>> KnownMudMaterials;

	FB1MudSimOutput SimOutput;

	uint32 SinkResetSerial = 0;

	/** Logs the per-wheel sink once the sim has applied the latest reset */
	bool bLogSinkReset = false;

	float UnstuckAssistTimeLeft = 0.0f;

	bool bInMud = false;
	float TimeSinceMudContact = 0.0f;
};
