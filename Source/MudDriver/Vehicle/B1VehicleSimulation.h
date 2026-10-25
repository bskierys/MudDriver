#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1VehicleSimInput.h"

/**
 * Physics-thread vehicle simulation. Environment forces (mud, current, winch) go here (ADR-003).
 * Never touch UObjects from here; game-thread data arrives through SetGameThreadInput.
 */
class FB1VehicleSimulation : public UChaosWheeledVehicleSimulation
{
public:

	/** Game thread. The physics thread picks up the latest value on its next step */
	void SetGameThreadInput(const FB1VehicleSimInput& Input);

	virtual void UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle) override;

private:

	void ApplyTestForce(const FB1VehicleSimInput& Input);

	FCriticalSection InputLock;
	FB1VehicleSimInput PendingInput;

	bool bLoggedFirstTick = false;
};
