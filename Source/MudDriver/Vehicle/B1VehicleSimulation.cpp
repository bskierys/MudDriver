#include "B1VehicleSimulation.h"
#include "B1Log.h"

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

void FB1VehicleSimulation::UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle)
{
	UChaosWheeledVehicleSimulation::UpdateSimulation(DeltaTime, InputData, Handle);

	FB1VehicleSimInput Input;
	{
		FScopeLock Lock(&InputLock);
		Input = PendingInput;
	}

	ApplyTestForce(Input);

	if (!bLoggedFirstTick)
	{
		bLoggedFirstTick = true;
		UE_LOG(LogB1Physics, Log, TEXT("FB1VehicleSimulation ticking: dt=%.4f game thread=%d thread id=%u"),
			DeltaTime, IsInGameThread() ? 1 : 0, FPlatformTLS::GetCurrentThreadId());
	}
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
