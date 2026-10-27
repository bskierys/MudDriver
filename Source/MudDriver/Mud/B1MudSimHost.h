#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1MudSimHost.generated.h"

class FB1MudSimulation;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UB1MudSimHost : public UInterface
{
	GENERATED_BODY()
};

/** What UB1MudVehicleComponent needs from the vehicle's movement component. Its Chaos sim calls the FB1MudSimulation hooks */
class MUDDRIVER_API IB1MudSimHost
{
	GENERATED_BODY()

public:

	/** Null until the physics vehicle exists */
	virtual FB1MudSimulation* GetMudSimulation() const = 0;

	virtual int32 GetMudWheelCount() const = 0;
	virtual const FWheelStatus& GetMudWheelState(int32 WheelIndex) const = 0;

	/** World location of the wheel centre */
	virtual FVector GetMudWheelLocation(int32 WheelIndex) const = 0;

	/** cm */
	virtual float GetMudWheelRadius(int32 WheelIndex) const = 0;
	virtual float GetMudWheelWidth(int32 WheelIndex) const = 0;

	/** Throttle as the engine sees it, [0, 1] */
	virtual float GetMudEffectiveThrottle() const = 0;

	/** Raw player drive input, throttle minus brake, [-1, 1]; ignores cut-offs */
	virtual float GetMudDriveInput() const = 0;

	/** Called every physics step. While true the vehicle must not sleep and should wake if asleep: mud on a slope has to be able to slide it */
	virtual void SetMudKeepAwake(bool bKeepAwake) = 0;
};
