#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1VehicleSimInput.h"
#include "B1VehicleMovementComponent.generated.h"

class FB1VehicleSimulation;

/** Wheeled movement that runs FB1VehicleSimulation on the physics thread */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1VehicleMovementComponent : public UChaosWheeledVehicleMovementComponent
{
	GENERATED_BODY()

public:

	virtual TUniquePtr<Chaos::FSimpleWheeledVehicle> CreatePhysicsVehicle() override;

	/** Spring mode anchors at the vehicle's current location */
	void SetTestForce(EB1TestForce Mode);
	EB1TestForce GetTestForce() const { return TestForce; }
	const FVector& GetTestForceAnchor() const { return TestForceAnchor; }

	UPROPERTY(EditAnywhere, Category = "TestForce|Config")
	FB1TestForceSettings TestForceSettings;

protected:

	/** Game thread, while the async input for the next physics step is open */
	virtual void Update(float DeltaTime) override;

private:

	FB1VehicleSimulation* GetB1Simulation() const;

	EB1TestForce TestForce = EB1TestForce::Off;
	FVector TestForceAnchor = FVector::ZeroVector;

	/** Vehicle sleep stops the simulation step, so it's disabled while a test force is active */
	float SavedSleepThreshold = 0.0f;
};
