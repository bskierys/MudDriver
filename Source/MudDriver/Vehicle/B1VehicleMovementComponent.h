#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1VehicleSimInput.h"
#include "B1MudSimHost.h"
#include "B1EngineLoadSource.h"
#include "B1VehicleMovementComponent.generated.h"

class FB1VehicleSimulation;

/** Why the engine gives no torque; any reason cuts the throttle */
enum class EB1EngineCut : uint8
{
	None = 0,
	OutOfFuel = 1 << 0,
	EngineOff = 1 << 1,
};
ENUM_CLASS_FLAGS(EB1EngineCut);

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

	/** Sent with the next physics input; the body stays awake while attached */
	void SetWinchInput(const FB1WinchSimInput& Input);
	FB1WinchSimOutput GetWinchOutput() const;

	/** IB1EngineLoadSource: engine throttle² × torque(RPM) × RPM over the curve's peak power, [0, 1]; updated each game frame */
	virtual float GetNormalizedEnginePower() const override { return NormalizedEnginePower; }

	virtual bool IsEngineRunning() const override { return !EnumHasAnyFlags(EngineCut, EB1EngineCut::EngineOff); }

	/** Throttle as the engine sees it this frame, after rise/fall rates and the engine cut */
	float GetEffectiveThrottle() const { return EffectiveThrottle; }

	/** While any reason is set the throttle is forced to 0; brake, steering and handbrake still work */
	void SetEngineCut(EB1EngineCut Reason, bool bCut);
	bool IsEngineCut() const { return EngineCut != EB1EngineCut::None; }

	/** Unpossessed, Chaos replays the last replicated inputs, so the driver's last throttle would stay on */
	void ClearDriverInput();

	/** Locked wheels still creep on a firm slope, so a parked vehicle sleeps below ParkedSleepThreshold; mud slopes never sleep */
	void SetParkSleep(bool bParked);

	UPROPERTY(EditAnywhere, Category = "Vehicle|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float ParkedSleepThreshold = 50.0f;

	// IB1MudSimHost
	virtual FB1MudSimulation* GetMudSimulation() const override;
	virtual int32 GetMudWheelCount() const override { return GetNumWheels(); }
	virtual const FWheelStatus& GetMudWheelState(int32 WheelIndex) const override { return GetWheelState(WheelIndex); }
	virtual FVector GetMudWheelLocation(int32 WheelIndex) const override;
	virtual float GetMudWheelRadius(int32 WheelIndex) const override;
	virtual float GetMudWheelWidth(int32 WheelIndex) const override;
	virtual float GetMudEffectiveThrottle() const override { return EffectiveThrottle; }
	virtual float GetMudDriveInput() const override;
	virtual void SetMudKeepAwake(bool bKeepAwake) override;

protected:

	/** Game thread, while the async input for the next physics step is open */
	virtual void Update(float DeltaTime) override;

private:

	FB1VehicleSimulation* GetB1Simulation() const;

	void UpdateEnginePower();

	/** Test force and winch need every step simulated; parking sleeps sooner */
	void RefreshSleepThreshold();

	float NormalizedEnginePower = 0.0f;
	float EffectiveThrottle = 0.0f;

	/** torque × RPM at the torque curve's best point; the curve doesn't change at runtime */
	float PeakEnginePower = 0.0f;

	EB1EngineCut EngineCut = EB1EngineCut::None;

	EB1TestForce TestForce = EB1TestForce::Off;
	FVector TestForceAnchor = FVector::ZeroVector;

	FB1WinchSimInput WinchInput;

	/** Vehicle sleep stops the simulation step, so it's disabled while a test force or the winch is active */
	bool bForceKeepAwake = false;
	bool bParkSleep = false;
	bool bAppliedParkSleep = false;
	/** SleepThreshold before an override, restored when none is left */
	TOptional<float> DefaultSleepThreshold;

	/** Mud keep-awake blocks sleep through SleepSlopeLimit, so it doesn't fight the test force over SleepThreshold */
	bool bMudKeepAwake = false;
	float SavedSleepSlopeLimit = 0.0f;
};
