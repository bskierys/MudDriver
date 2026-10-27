#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1VehicleSimInput.h"
#include "B1MudSimHost.h"
#include "B1EngineLoadSource.h"
#include "B1VehicleMovementComponent.generated.h"

class FB1VehicleSimulation;

/** Wheeled movement that runs FB1VehicleSimulation on the physics thread */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1VehicleMovementComponent : public UChaosWheeledVehicleMovementComponent, public IB1MudSimHost, public IB1EngineLoadSource
{
	GENERATED_BODY()

public:

	UB1VehicleMovementComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual TUniquePtr<Chaos::FSimpleWheeledVehicle> CreatePhysicsVehicle() override;

	/** Spring mode anchors at the vehicle's current location */
	void SetTestForce(EB1TestForce Mode);
	EB1TestForce GetTestForce() const { return TestForce; }
	const FVector& GetTestForceAnchor() const { return TestForceAnchor; }

	UPROPERTY(EditAnywhere, Category = "TestForce|Config")
	FB1TestForceSettings TestForceSettings;

	/** IB1EngineLoadSource: engine throttle² × torque(RPM) × RPM over the curve's peak power, [0, 1]; updated each game frame */
	virtual float GetNormalizedEnginePower() const override { return NormalizedEnginePower; }

	/** Throttle as the engine sees it this frame, after rise/fall rates and the fuel cut-off */
	float GetEffectiveThrottle() const { return EffectiveThrottle; }

	/** Out of fuel: throttle is forced to 0, brake/steering/handbrake still work */
	void SetFuelCutOff(bool bCutOff) { bFuelCutOff = bCutOff; }
	bool IsFuelCutOff() const { return bFuelCutOff; }

	// IB1MudSimHost
	virtual FB1MudSimulation* GetMudSimulation() const override;
	virtual int32 GetMudWheelCount() const override { return GetNumWheels(); }
	virtual const FWheelStatus& GetMudWheelState(int32 WheelIndex) const override { return GetWheelState(WheelIndex); }
	virtual FVector GetMudWheelLocation(int32 WheelIndex) const override;
	virtual float GetMudWheelRadius(int32 WheelIndex) const override;
	virtual float GetMudWheelWidth(int32 WheelIndex) const override;
	virtual float GetMudEffectiveThrottle() const override { return EffectiveThrottle; }
	virtual float GetMudDriveInput() const override;

protected:

	/** Game thread, while the async input for the next physics step is open */
	virtual void Update(float DeltaTime) override;

private:

	FB1VehicleSimulation* GetB1Simulation() const;

	void UpdateEnginePower();

	float NormalizedEnginePower = 0.0f;
	float EffectiveThrottle = 0.0f;

	/** torque × RPM at the torque curve's best point; the curve doesn't change at runtime */
	float PeakEnginePower = 0.0f;

	bool bFuelCutOff = false;

	EB1TestForce TestForce = EB1TestForce::Off;
	FVector TestForceAnchor = FVector::ZeroVector;

	/** Vehicle sleep stops the simulation step, so it's disabled while a test force is active */
	float SavedSleepThreshold = 0.0f;
};
