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

	/** Mud traction and rolling resistance vs sink depth (ADR-001) */
	virtual void ApplyWheelFrictionForces(float DeltaTime) override;

private:

	void ApplyTestForce(const FB1VehicleSimInput& Input);
	void UpdateRelativeLoads();
	void UpdateSinkDepth(float DeltaTime);
	float GetNormalizedDepth(int32 WheelIdx, const FB1MudSimParams& Mud) const;
	void UpdateSinkOffset(int32 NumWheels);
	void ApplyRollingResistance(float DeltaTime);
	void ApplyBellyDrag(float DeltaTime);

	/** Null when the wheel is airborne or not on a known mud material */
	const FB1MudSimParams* FindContactMud(int32 WheelIdx) const;
	const FB1MudSimParams* FindMud(const TWeakObjectPtr<UPhysicalMaterial>& Material) const;

	/** Distance from the mud surface down to the first hard surface, or MAX_flt if none within MaxDepth */
	float TraceHardGround(const FVector& SurfacePoint, const FVector& Down, float MaxDepth, const FCollisionQueryParams& TraceParams, const FCollisionResponseParams& ResponseParams) const;

	FCriticalSection InputLock;
	FB1VehicleSimInput PendingInput;

	/** Copy of PendingInput for the current step */
	FB1VehicleSimInput StepInput;

	/** Per wheel, load over the average load of the wheels in contact, capped */
	TArray<float> RelativeLoad;

	int32 NumWheelsInContact = 0;

	TArray<float> MudFrictionScale;
	TArray<float> RollingResistance;
	float BellyDepth = 0.0f;
	float BellyDrag = 0.0f;

	mutable FCriticalSection OutputLock;
	FB1VehicleSimOutput LatestOutput;

	/** Per wheel, cm */
	TArray<float> SinkDepth;

	/** Per wheel, cm. SinkDepth with a rise limit, so leaving mud doesn't pop the suspension */
	TArray<float> SinkOffset;

	/** Per wheel, cm below the mud surface. Caps sinking where the bottom ramps up or the landscape is shallow */
	TArray<float> HardGroundDepth;

	float LastDeltaTime = 0.0f;

	bool bLoggedFirstTick = false;
};
