#include "B1VehicleSimulation.h"
#include "B1Log.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

namespace
{
	constexpr float CmPerM = 100.0f;
	/** Chaos forces are in kg*cm/s^2 */
	constexpr float NewtonsToUnreal = 100.0f;
}

void FB1VehicleSimulation::SetGameThreadInput(const FB1VehicleSimInput& Input)
{
	FScopeLock Lock(&InputLock);
	PendingInput = Input;
}

FB1WinchSimOutput FB1VehicleSimulation::GetGameThreadWinchOutput() const
{
	FScopeLock Lock(&OutputLock);
	return LatestWinchOutput;
}

void FB1VehicleSimulation::UpdateWinch(const FB1WinchSimInput& Input, const Chaos::FRigidBodyHandle_Internal* Handle, float DeltaTime)
{
	Winch.Tension = 0.0f;
	Winch.Force = FVector::ZeroVector;
	AnchorHandle = nullptr;
	if (!Input.bAttached || !Handle)
	{
		Winch.RestLength = 0.0f;
		Winch.Length = 0.0f;
		return;
	}

	if (Input.AttachSerial != Winch.AttachSerial)
	{
		Winch.AttachSerial = Input.AttachSerial;
		Winch.RestLength = Input.InitialRestLength;
	}

	// From this step's body pose: a game-thread position would be a frame old.
	const FTransform BodyTransform(Handle->R(), Handle->X());
	WinchPoint = BodyTransform.TransformPosition(Input.WinchPointLocal);

	// A simulating anchor moves, so its point and velocity come from this step too.
	Chaos::FRigidBodyHandle_Internal* AnchorBody = Input.AnchorProxy ? Input.AnchorProxy->GetPhysicsThreadAPI() : nullptr;
	FVector AnchorVelocity = FVector::ZeroVector;
	AnchorPoint = Input.AnchorLocation;
	if (AnchorBody && AnchorBody->CanTreatAsRigid())
	{
		const FTransform AnchorTransform(AnchorBody->R(), AnchorBody->X());
		AnchorPoint = AnchorTransform.TransformPosition(Input.AnchorPointLocal);
		const FVector AnchorCenterOfMass = AnchorTransform.TransformPosition(AnchorBody->CenterOfMass());
		AnchorVelocity = AnchorBody->V() + FVector::CrossProduct(AnchorBody->W(), AnchorPoint - AnchorCenterOfMass);
	}
	else
	{
		AnchorBody = nullptr;
	}

	const FVector ToAnchor = AnchorPoint - WinchPoint;
	Winch.Length = ToAnchor.Size();
	if (Winch.Length < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Direction = ToAnchor / Winch.Length;

	// The drum stalls at the cap, so the cable never stores more stretch than MaxPullForce.
	const float SpringForce = Input.Stiffness * (Winch.Length - Winch.RestLength) / CmPerM;
	const float PreviousRestLength = Winch.RestLength;
	if (Input.Pull > 0.0f && SpringForce < Input.MaxPullForce)
	{
		Winch.RestLength = FMath::Max(Winch.RestLength - Input.ReelInSpeed * Input.Pull * DeltaTime, Input.MinRestLength);
	}
	// Reel-out ignores the stall, so a jeep hanging on the cable descends at reel speed.
	else if (Input.Pull < 0.0f)
	{
		Winch.RestLength = FMath::Max(Winch.RestLength, FMath::Min(Winch.RestLength - Input.ReelOutSpeed * Input.Pull * DeltaTime, Input.MaxCableLength));
	}

	const float Stretch = (Winch.Length - Winch.RestLength) / CmPerM;
	if (Stretch <= 0.0f)
	{
		return;
	}

	const FVector CenterOfMass = BodyTransform.TransformPosition(Handle->CenterOfMass());
	const FVector PointVelocity = Handle->V() + FVector::CrossProduct(Handle->W(), WinchPoint - CenterOfMass);
	const float ClosingSpeed = FVector::DotProduct(PointVelocity - AnchorVelocity, Direction) / CmPerM;
	// On reel-out, damp the stretch rate: a jeep following the drum doesn't stretch the cable. Reel-in keeps body-speed damping so tension ramps (ADR-007).
	const float PayoutRate = FMath::Max(0.0f, Winch.RestLength - PreviousRestLength);
	const float RestRate = DeltaTime > 0.0f ? PayoutRate / DeltaTime / CmPerM : 0.0f;
	const float StretchRate = -ClosingSpeed - RestRate;

	// One-sided: damping can weaken the pull but never push.
	Winch.Tension = FMath::Clamp(Input.Stiffness * Stretch + Input.Damping * StretchRate, 0.0f, Input.MaxPullForce);
	Winch.Force = Direction * Winch.Tension;
	AnchorHandle = Winch.Tension > 0.0f ? AnchorBody : nullptr;
}

void FB1VehicleSimulation::ApplyDeferredForces(Chaos::FRigidBodyHandle_Internal* Handle)
{
	UChaosWheeledVehicleSimulation::ApplyDeferredForces(Handle);

	// Cleared so a sleeping jeep (no UpdateSimulation) doesn't keep pushing the log with a stale force.
	if (!AnchorHandle)
	{
		return;
	}
	const FVector Force = -Winch.Force * NewtonsToUnreal;
	const FVector CenterOfMass = FTransform(AnchorHandle->R(), AnchorHandle->X()).TransformPosition(AnchorHandle->CenterOfMass());
	AnchorHandle->AddForce(Force);
	AnchorHandle->AddTorque(FVector::CrossProduct(AnchorPoint - CenterOfMass, Force));
	AnchorHandle = nullptr;
}

void FB1VehicleSimulation::UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle)
{
	{
		FScopeLock Lock(&InputLock);
		StepInput = PendingInput;
	}
	Mud.BeginStep();
	UpdateWinch(StepInput.Winch, Handle, DeltaTime);
	Mud.SetExternalForce(Winch.Force);

	UChaosWheeledVehicleSimulation::UpdateSimulation(DeltaTime, InputData, Handle);

	ApplyTestForce(StepInput);
	if (Winch.Tension > 0.0f)
	{
		AddForceAtPosition(Winch.Force * NewtonsToUnreal, WinchPoint);
	}
	Mud.EndStep(*this, DeltaTime);

	{
		FScopeLock Lock(&OutputLock);
		LatestWinchOutput = Winch;
	}

	if (!bLoggedFirstTick)
	{
		bLoggedFirstTick = true;
		UE_LOG(LogB1Physics, Log, TEXT("FB1VehicleSimulation ticking: dt=%.4f game thread=%d thread id=%u"),
			DeltaTime, IsInGameThread() ? 1 : 0, FPlatformTLS::GetCurrentThreadId());
	}
}

void FB1VehicleSimulation::PerformSuspensionTraces(const TArray<Chaos::FSuspensionTrace>& SuspensionTrace, FCollisionQueryParams& TraceParams, FCollisionResponseContainer& CollisionResponse, TArray<FWheelTraceParams>& WheelTraceParams)
{
	TArray<Chaos::FSuspensionTrace> Traces = SuspensionTrace;
	Mud.PreSuspensionTraces(*this, Traces);
	UChaosWheeledVehicleSimulation::PerformSuspensionTraces(Traces, TraceParams, CollisionResponse, WheelTraceParams);
	Mud.PostSuspensionTraces(*this, Traces, TraceParams, CollisionResponse);
}

void FB1VehicleSimulation::ApplyWheelFrictionForces(float DeltaTime)
{
	Mud.PreFriction(*this);
	UChaosWheeledVehicleSimulation::ApplyWheelFrictionForces(DeltaTime);
	Mud.PostFriction(*this, DeltaTime);
}

void FB1VehicleSimulation::ApplyTestForce(const FB1VehicleSimInput& Input)
{
	const FB1TestForceSettings& Settings = Input.TestForceSettings;

	switch (Input.TestForce)
	{
	case EB1TestForce::Push:
		AddForce(FVector(0.0f, Settings.PushForce * NewtonsToUnreal, 0.0f));
		break;

	case EB1TestForce::Spring:
	{
		const FVector ToAnchor = Input.TestForceAnchor - VehicleState.VehicleWorldTransform.GetLocation();
		const float Distance = ToAnchor.Size();
		const float Stretch = Distance / CmPerM - Settings.SpringRestLength;
		if (Stretch <= 0.0f)
		{
			break;
		}

		// One-sided: the cable only pulls, so damping can weaken the pull but never push.
		const FVector Direction = ToAnchor / Distance;
		const float ClosingSpeed = FVector::DotProduct(VehicleState.VehicleWorldVelocity, Direction) / CmPerM;
		const float Force = FMath::Clamp(Settings.SpringStiffness * Stretch - Settings.SpringDamping * ClosingSpeed, 0.0f, Settings.SpringMaxForce);
		AddForce(Direction * Force * NewtonsToUnreal);
		break;
	}

	default:
		break;
	}
}
