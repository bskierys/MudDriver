#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1VehicleSimInput.h"
#include "B1VehicleMovementComponent.generated.h"

class FB1VehicleSimulation;
class UB1PhysicalMaterial_Mud;

/** Wheeled movement that runs FB1VehicleSimulation on the physics thread */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1VehicleMovementComponent : public UChaosWheeledVehicleMovementComponent
{
	GENERATED_BODY()

public:

	UB1VehicleMovementComponent(const FObjectInitializer& ObjectInitializer);

	virtual TUniquePtr<Chaos::FSimpleWheeledVehicle> CreatePhysicsVehicle() override;

	/** Spring mode anchors at the vehicle's current location */
	void SetTestForce(EB1TestForce Mode);
	EB1TestForce GetTestForce() const { return TestForce; }
	const FVector& GetTestForceAnchor() const { return TestForceAnchor; }

	UPROPERTY(EditAnywhere, Category = "TestForce|Config")
	FB1TestForceSettings TestForceSettings;

	/** cm, from the latest physics step; 0 off mud */
	float GetWheelSinkDepth(int32 WheelIndex) const;

	/** Mud friction curve value from the latest physics step; 1 off mud */
	float GetWheelMudFrictionScale(int32 WheelIndex) const;

	/** N, from the latest physics step */
	float GetWheelRollingResistance(int32 WheelIndex) const;

	/** cm the average sink depth is past BellyClearance */
	float GetBellyDepth() const { return SimOutput.BellyDepth; }

	/** N */
	float GetBellyDrag() const { return SimOutput.BellyDrag; }

	/** Engine throttle × torque(RPM) × RPM over the curve's peak power, [0, 1]; updated each game frame */
	float GetNormalizedEnginePower() const { return NormalizedEnginePower; }

	/** Throttle as the engine sees it this frame, after rise/fall rates and the fuel cut-off */
	float GetEffectiveThrottle() const { return EffectiveThrottle; }

	/** cm, mean over all wheels */
	float GetAverageSinkDepth() const;

	/** B1Unstuck: teleports the vehicle level, wheels 50 cm above the mud under it, stops it and clears every wheel's sink.
	 *  Then for UnstuckAssistDuration, throttle/brake push the vehicle along its heading */
	void Unstuck();

	bool IsUnstuckAssistActive() const { return UnstuckAssistTimeLeft > 0.0f; }

	/** Push at full throttle during the B1Unstuck window, along the flattened heading */
	UPROPERTY(EditAnywhere, Category = "Mud|Unstuck", meta = (Units = "N", ClampMin = "0"))
	float UnstuckAssistForce = 15000.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Unstuck", meta = (Units = "s", ClampMin = "0"))
	float UnstuckAssistDuration = 4.0f;

	/** Out of fuel: throttle is forced to 0, brake/steering/handbrake still work */
	void SetFuelCutOff(bool bCutOff) { bFuelCutOff = bCutOff; }
	bool IsFuelCutOff() const { return bFuelCutOff; }

	/** Belly drag starts once the average sink depth passes it. Axle height, below the jeep body's ~62 cm underside */
	UPROPERTY(EditAnywhere, Category = "Mud|Config", meta = (Units = "cm", ClampMin = "0"))
	float BellyClearance = 45.0f;

protected:

	/** Game thread, while the async input for the next physics step is open */
	virtual void Update(float DeltaTime) override;

private:

	FB1VehicleSimulation* GetB1Simulation() const;

	/** Adds the mud materials the wheels touch now; params of every known one go to the sim */
	void GatherMudParams(TArray<FB1MudSimParams>& OutParams);

	/** Logs the chassis underside height over firm ground once, to check BellyClearance */
	void LogMeasuredClearance();
	bool bLoggedClearance = false;

	/** Grows as wheels touch new mud. The sim treats a material as firm until it's in here, about a frame late */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UB1PhysicalMaterial_Mud>> KnownMudMaterials;

	FB1VehicleSimOutput SimOutput;

	void UpdateEnginePower();

	float NormalizedEnginePower = 0.0f;
	float EffectiveThrottle = 0.0f;

	/** torque × RPM at the torque curve's best point; the curve doesn't change at runtime */
	float PeakEnginePower = 0.0f;

	bool bFuelCutOff = false;

	uint32 SinkResetSerial = 0;

	/** Logs the per-wheel sink once the sim has applied the latest reset */
	bool bLogSinkReset = false;

	float UnstuckAssistTimeLeft = 0.0f;
	FVector GetUnstuckAssistForce(float DeltaTime);

	EB1TestForce TestForce = EB1TestForce::Off;
	FVector TestForceAnchor = FVector::ZeroVector;

	/** Vehicle sleep stops the simulation step, so it's disabled while a test force is active */
	float SavedSleepThreshold = 0.0f;
};
