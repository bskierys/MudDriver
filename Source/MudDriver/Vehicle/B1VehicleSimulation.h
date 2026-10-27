#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1VehicleSimInput.h"
#include "B1MudSimulation.h"

/**
 * Physics-thread vehicle simulation. Environment forces (mud, current, winch) go here (ADR-003).
 * Never touch UObjects from here; game-thread data arrives through SetGameThreadInput.
 */
class FB1VehicleSimulation : public UChaosWheeledVehicleSimulation
{
public:

	/** Game thread. The physics thread picks up the latest value on its next step */
	void SetGameThreadInput(const FB1VehicleSimInput& Input);

	/** Game thread. Result of the latest physics step */
	FB1WinchSimOutput GetGameThreadWinchOutput() const;

	FB1MudSimulation& GetMudSimulation() { return Mud; }

	virtual void UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle) override;

	virtual void PerformSuspensionTraces(const TArray<Chaos::FSuspensionTrace>& SuspensionTrace, FCollisionQueryParams& TraceParams, FCollisionResponseContainer& CollisionResponse, TArray<FWheelTraceParams>& WheelTraceParams) override;

	virtual void ApplyWheelFrictionForces(float DeltaTime) override;

private:

	void ApplyTestForce(const FB1VehicleSimInput& Input);

	/** Before the base step, from the body handle: reels in and sets WinchForce / WinchPoint */
	void UpdateWinch(const FB1WinchSimInput& Input, const Chaos::FRigidBodyHandle_Internal* Handle, float DeltaTime);

	FB1MudSimulation Mud;

	/** Physics thread; the rest length shrinks while reeling in */
	FB1WinchSimOutput Winch;
	FVector WinchPoint = FVector::ZeroVector;

	mutable FCriticalSection OutputLock;
	FB1WinchSimOutput LatestWinchOutput;

	FCriticalSection InputLock;
	FB1VehicleSimInput PendingInput;

	/** Copy of PendingInput for the current step */
	FB1VehicleSimInput StepInput;

	bool bLoggedFirstTick = false;
};
