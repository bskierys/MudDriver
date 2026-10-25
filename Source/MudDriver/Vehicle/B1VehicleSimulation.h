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

	/** Game thread. Result of the latest physics step */
	FB1VehicleSimOutput GetGameThreadOutput() const;

	virtual void UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle) override;

	/** Sinking (ADR-012): moves each wheel's hit down by its sink offset, so the suspension extends */
	virtual void PerformSuspensionTraces(const TArray<Chaos::FSuspensionTrace>& SuspensionTrace, FCollisionQueryParams& TraceParams, FCollisionResponseContainer& CollisionResponse, TArray<FWheelTraceParams>& WheelTraceParams) override;

private:

	void ApplyTestForce(const FB1VehicleSimInput& Input);
	void UpdateSinkDepth(float DeltaTime, const FB1VehicleSimInput& Input);
	void UpdateSinkOffset(int32 NumWheels);

	FCriticalSection InputLock;
	FB1VehicleSimInput PendingInput;

	mutable FCriticalSection OutputLock;
	FB1VehicleSimOutput LatestOutput;

	/** Per wheel, cm */
	TArray<float> SinkDepth;

	/** Per wheel, cm. SinkDepth with a rise limit, so leaving mud doesn't pop the suspension */
	TArray<float> SinkOffset;

	float LastDeltaTime = 0.0f;

	bool bLoggedFirstTick = false;
};
