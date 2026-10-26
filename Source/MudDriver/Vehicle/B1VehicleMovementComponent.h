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

	EB1TestForce TestForce = EB1TestForce::Off;
	FVector TestForceAnchor = FVector::ZeroVector;

	/** Vehicle sleep stops the simulation step, so it's disabled while a test force is active */
	float SavedSleepThreshold = 0.0f;
};
