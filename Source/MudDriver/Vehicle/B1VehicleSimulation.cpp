#include "B1VehicleSimulation.h"
#include "B1Log.h"

namespace
{
	constexpr float CmPerM = 100.0f;
	/** Chaos forces are in kg*cm/s^2 */
	constexpr float NewtonsToUnreal = 100.0f;
	/** cm/s, applied only while rising, so leaving mud doesn't pop the suspension */
	constexpr float SinkOffsetMaxRiseSpeed = 30.0f;
	/** Caps the load scaling of sinking, so a hard landing doesn't bury the wheel */
	constexpr float MaxLoadFactor = 1.5f;
}

void FB1VehicleSimulation::SetGameThreadInput(const FB1VehicleSimInput& Input)
{
	FScopeLock Lock(&InputLock);
	PendingInput = Input;
}

FB1VehicleSimOutput FB1VehicleSimulation::GetGameThreadOutput() const
{
	FScopeLock Lock(&OutputLock);
	return LatestOutput;
}

void FB1VehicleSimulation::UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle)
{
	UChaosWheeledVehicleSimulation::UpdateSimulation(DeltaTime, InputData, Handle);

	FB1VehicleSimInput Input;
	{
		FScopeLock Lock(&InputLock);
		Input = PendingInput;
	}

	LastDeltaTime = DeltaTime;
	ApplyTestForce(Input);
	UpdateSinkDepth(DeltaTime, Input);

	{
		FScopeLock Lock(&OutputLock);
		LatestOutput.SinkDepth = SinkDepth;
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
	UpdateSinkOffset(SuspensionTrace.Num());

	// A sunk chassis drags the trace start down into the thin mud slab, so trace from a start raised by the
	// offset. Chaos reads Distance as measured from the real start, which makes the hit exactly Offset deeper.
	TArray<Chaos::FSuspensionTrace> RaisedTrace = SuspensionTrace;
	for (int32 WheelIdx = 0; WheelIdx < RaisedTrace.Num(); ++WheelIdx)
	{
		Chaos::FSuspensionTrace& Trace = RaisedTrace[WheelIdx];
		Trace.Start -= (Trace.End - Trace.Start).GetSafeNormal() * SinkOffset[WheelIdx];
	}

	UChaosWheeledVehicleSimulation::PerformSuspensionTraces(RaisedTrace, TraceParams, CollisionResponse, WheelTraceParams);

	for (int32 WheelIdx = 0; WheelIdx < RaisedTrace.Num(); ++WheelIdx)
	{
		FHitResult& Hit = WheelState.TraceResult[WheelIdx];
		const float Offset = SinkOffset[WheelIdx];
		if (!Hit.bBlockingHit || Offset <= 0.0f)
		{
			continue;
		}

		// Past full droop the wheel keeps contact at max length and the chassis settles on the hard bottom.
		const FVector Down = (RaisedTrace[WheelIdx].End - RaisedTrace[WheelIdx].Start).GetSafeNormal();
		const float RealSweepLength = (Hit.TraceEnd - Hit.TraceStart).Size() - Offset;
		const float SurfaceDistance = Hit.Distance - Offset;
		Hit.Distance = FMath::Min(Hit.Distance, RealSweepLength);

		const float Push = Hit.Distance - SurfaceDistance;
		Hit.Location += Down * Push;
		Hit.ImpactPoint += Down * Push;
	}
}

void FB1VehicleSimulation::UpdateSinkOffset(int32 NumWheels)
{
	// SinkDepth is from the previous step; traces run before UpdateSimulation.
	SinkOffset.SetNumZeroed(NumWheels);
	const float MaxRise = SinkOffsetMaxRiseSpeed * LastDeltaTime;

	// Sinking is already rate-limited by the mud rates; only rising needs a limit.
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const float Target = SinkDepth.IsValidIndex(WheelIdx) ? SinkDepth[WheelIdx] : 0.0f;
		SinkOffset[WheelIdx] = FMath::Max(Target, SinkOffset[WheelIdx] - MaxRise);
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

void FB1VehicleSimulation::UpdateSinkDepth(float DeltaTime, const FB1VehicleSimInput& Input)
{
	const int32 NumWheels = PVehicle->Wheels.Num();
	SinkDepth.SetNumZeroed(NumWheels);

	// Chaos's reported load doesn't match RestingForce at rest, so loads are relative to the average.
	float LoadSum = 0.0f;
	int32 NumLoaded = 0;
	for (const Chaos::FSimpleWheelSim& PWheel : PVehicle->Wheels)
	{
		if (PWheel.InContact())
		{
			LoadSum += PWheel.GetWheelLoadForce();
			++NumLoaded;
		}
	}
	const float AverageLoad = NumLoaded > 0 ? LoadSum / NumLoaded : 0.0f;

	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const Chaos::FSimpleWheelSim& PWheel = PVehicle->Wheels[WheelIdx];

		// An airborne wheel keeps its depth, so a bounce in the mud doesn't reset it.
		if (!PWheel.InContact())
		{
			continue;
		}

		const TWeakObjectPtr<UPhysicalMaterial>& ContactMaterial = WheelState.TraceResult[WheelIdx].PhysMaterial;
		const FB1MudSimParams* Mud = Input.MudMaterials.FindByPredicate([&ContactMaterial](const FB1MudSimParams& Params)
		{
			return Params.Material == ContactMaterial;
		});

		if (!Mud)
		{
			SinkDepth[WheelIdx] = 0.0f;
			continue;
		}

		// Weight sets a static floor; wheelspin digs below it, rolling with grip climbs back to it.
		const float Load = AverageLoad > 0.0f ? FMath::Clamp(PWheel.GetWheelLoadForce() / AverageLoad, 0.0f, MaxLoadFactor) : 1.0f;
		const float Floor = FMath::Min(Mud->StaticSinkDepth * Load, Mud->MaxSinkDepth);

		const float SlipSpeed = FMath::Abs(PWheel.GetSlipMagnitude()) / CmPerM;
		const float Spin = FMath::Clamp(SlipSpeed / Mud->FullSinkSlipSpeed, 0.0f, 1.0f);
		const float RolledDistance = FMath::Abs(PWheel.GetRoadSpeed()) / CmPerM * DeltaTime;

		float Depth = SinkDepth[WheelIdx];
		if (Depth < Floor)
		{
			Depth = FMath::Min(Depth + Mud->SettleRate * DeltaTime, Floor);
		}
		Depth += Mud->SinkRate * Spin * Load * DeltaTime;
		if (Depth > Floor)
		{
			Depth = FMath::Max(Depth - Mud->RiseRate * RolledDistance * (1.0f - Spin), Floor);
		}
		SinkDepth[WheelIdx] = FMath::Clamp(Depth, 0.0f, Mud->MaxSinkDepth);
	}
}
