#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1MudSimTypes.h"

/**
 * Mud on the physics thread (ADR-001, ADR-012): sinking, traction, rolling resistance and belly drag.
 * A member of the host's Chaos simulation, which calls the hooks below in order each step.
 */
class MUDDRIVER_API FB1MudSimulation
{
public:

	/** Game thread, with the host simulation */
	FB1MudSimulation();

	/** Game thread. The physics thread picks up the latest value on its next step */
	void SetGameThreadInput(const FB1MudSimInput& Input);

	/** Game thread. Result of the latest physics step */
	FB1MudSimOutput GetGameThreadOutput() const;

	/** Before the base UpdateSimulation: latches the input and applies a pending sink reset */
	void BeginStep();

	/** Physics thread, after BeginStep: a force the host applies this step (e.g. a winch), world, N. Counted as push in the drag caps */
	void SetExternalForce(const FVector& Force) { ExternalForce = Force; }

	/** Raises each trace start by the wheel's sink offset; pass the result to the base PerformSuspensionTraces */
	void PreSuspensionTraces(const UChaosWheeledVehicleSimulation& Vehicle, TArray<Chaos::FSuspensionTrace>& Traces);

	/** Pushes the hits down by the sink offset, capped by the hard ground under the mud */
	void PostSuspensionTraces(UChaosWheeledVehicleSimulation& Vehicle, const TArray<Chaos::FSuspensionTrace>& RaisedTraces, const FCollisionQueryParams& TraceParams, const FCollisionResponseContainer& CollisionResponse);

	/** Scales mud wheels' FrictionMultiplier before the base ApplyWheelFrictionForces */
	void PreFriction(UChaosWheeledVehicleSimulation& Vehicle);

	/** Restores the multipliers, then adds rolling resistance and belly drag */
	void PostFriction(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime);

	/** After the base UpdateSimulation: unstuck assist, sink depth update, output */
	void EndStep(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime);

private:

	void UpdateRelativeLoads(const UChaosWheeledVehicleSimulation& Vehicle);
	void UpdateSinkDepth(const UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime);
	void ApplyRollingResistance(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime);
	void ApplyBellyDrag(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime);

	float GetSinkDepth(int32 WheelIdx) const { return SinkDepth.IsValidIndex(WheelIdx) ? SinkDepth[WheelIdx] : 0.0f; }

	/** Null when the wheel is airborne or not on a known mud material */
	const FB1MudSimParams* FindContactMud(const UChaosWheeledVehicleSimulation& Vehicle, int32 WheelIdx) const;
	const FB1MudSimParams* FindMud(const TWeakObjectPtr<UPhysicalMaterial>& Material) const;

	/** Distance from the mud surface down to the first hard surface, or MAX_flt if none within MaxDepth */
	float TraceHardGround(const UWorld* World, const FVector& SurfacePoint, const FVector& Down, float MaxDepth, const FCollisionQueryParams& TraceParams, const FCollisionResponseParams& ResponseParams) const;

	FCriticalSection InputLock;
	FB1MudSimInput PendingInput;

	/** Copy of PendingInput for the current step */
	FB1MudSimInput StepInput;

	mutable FCriticalSection OutputLock;
	FB1MudSimOutput LatestOutput;

	/** Per wheel, load over the average load of the wheels in contact, capped */
	TArray<float> RelativeLoad;
	int32 NumWheelsInContact = 0;

	TArray<float> MudFrictionScale;
	TArray<float> SavedFrictionMultiplier;
	TArray<float> RollingResistance;
	TArray<float> SlopePull;
	TArray<float> WheelNetPush;
	TArray<float> TreadSpeed;
	float BellyDepth = 0.0f;
	float BellyDrag = 0.0f;
	float MudGroundUpZ = 1.0f;

	/** N; reset each BeginStep */
	FVector ExternalForce = FVector::ZeroVector;

	/** Per wheel, cm */
	TArray<float> SinkDepth;

	/** Per wheel, cm. SinkDepth with a rise limit, so leaving mud doesn't pop the suspension */
	TArray<float> SinkOffset;

	/** Per wheel, cm below the mud surface. Caps sinking where the bottom ramps up or the landscape is shallow */
	TArray<float> HardGroundDepth;

	float LastDeltaTime = 0.0f;

	uint32 AppliedSinkResetSerial = 0;

	/** UB1MudSettings::MudObjectChannel, copied on the game thread */
	ECollisionChannel MudChannel;
};
