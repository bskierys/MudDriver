#include "B1VehicleSimulation.h"
#include "B1Log.h"
#include "B1CollisionChannels.h"
#include "Engine/World.h"

namespace
{
	constexpr float CmPerM = 100.0f;
	/** Chaos forces are in kg*cm/s^2 */
	constexpr float NewtonsToUnreal = 100.0f;
	/** Rising is limited so leaving mud doesn't pop the suspension: cm/s at low speed, cm per cm rolled above it */
	constexpr float SinkOffsetMinRiseSpeed = 30.0f;
	constexpr float SinkOffsetRiseSlope = 0.5f;
	/** Caps the load scaling of sinking, so a hard landing doesn't bury the wheel */
	constexpr float MaxLoadFactor = 1.5f;
	constexpr float GravityCmPerS2 = 980.665f;
	/** Share of the wheel's stopping force rolling resistance may use per step, as Chaos does for braking */
	constexpr float MaxStopForceFraction = 0.4f;
	/** cm/s */
	constexpr float StandstillSpeed = 1.0f;
	constexpr float HardGroundTraceLift = 2.0f;
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
	{
		FScopeLock Lock(&InputLock);
		StepInput = PendingInput;
	}

	if (StepInput.SinkResetSerial != AppliedSinkResetSerial)
	{
		AppliedSinkResetSerial = StepInput.SinkResetSerial;
		SinkDepth.Init(0.0f, SinkDepth.Num());
		SinkOffset.Init(0.0f, SinkOffset.Num());
	}

	UChaosWheeledVehicleSimulation::UpdateSimulation(DeltaTime, InputData, Handle);

	LastDeltaTime = DeltaTime;
	ApplyTestForce(StepInput);
	if (!StepInput.UnstuckAssistForce.IsZero())
	{
		AddForce(StepInput.UnstuckAssistForce * NewtonsToUnreal);
	}
	UpdateSinkDepth(DeltaTime);

	{
		FScopeLock Lock(&OutputLock);
		LatestOutput.SinkDepth = SinkDepth;
		LatestOutput.MudFrictionScale = MudFrictionScale;
		LatestOutput.RollingResistance = RollingResistance;
		LatestOutput.BellyDepth = BellyDepth;
		LatestOutput.BellyDrag = BellyDrag;
		LatestOutput.SinkResetSerial = AppliedSinkResetSerial;
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

	HardGroundDepth.Init(MAX_flt, RaisedTrace.Num());
	FCollisionResponseParams GroundResponse(CollisionResponse);
	GroundResponse.CollisionResponse.SetResponse(B1_ObjectChannel_Mud, ECR_Ignore);

	for (int32 WheelIdx = 0; WheelIdx < RaisedTrace.Num(); ++WheelIdx)
	{
		FHitResult& Hit = WheelState.TraceResult[WheelIdx];
		if (!Hit.bBlockingHit)
		{
			continue;
		}

		const FVector Down = (RaisedTrace[WheelIdx].End - RaisedTrace[WheelIdx].Start).GetSafeNormal();
		const float RaisedBy = SinkOffset[WheelIdx];
		const float SurfaceDistance = Hit.Distance - RaisedBy;

		// Where the bottom ramps up (or the landscape is shallow) the wheel can't sink past it.
		if (const FB1MudSimParams* Mud = FindMud(Hit.PhysMaterial))
		{
			HardGroundDepth[WheelIdx] = TraceHardGround(Hit.ImpactPoint, Down, Mud->MaxSinkDepth, TraceParams, GroundResponse);
			SinkOffset[WheelIdx] = FMath::Min(SinkOffset[WheelIdx], HardGroundDepth[WheelIdx]);
		}

		const float Offset = SinkOffset[WheelIdx];
		if (Offset <= 0.0f)
		{
			Hit.Distance = SurfaceDistance;
			continue;
		}

		// Past full droop the wheel keeps contact at max length and the chassis settles on the hard bottom.
		const float RealSweepLength = (Hit.TraceEnd - Hit.TraceStart).Size() - RaisedBy;
		Hit.Distance = FMath::Min(SurfaceDistance + Offset, RealSweepLength);

		const float Push = Hit.Distance - SurfaceDistance;
		Hit.Location += Down * Push;
		Hit.ImpactPoint += Down * Push;
	}
}

void FB1VehicleSimulation::UpdateSinkOffset(int32 NumWheels)
{
	// SinkDepth is from the previous step; traces run before UpdateSimulation.
	SinkOffset.SetNumZeroed(NumWheels);

	// Sinking is already rate-limited by the mud rates; only rising needs a limit.
	// It climbs out over distance, like out of a rut, so a fast exit doesn't drive sunk into firm ground.
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const float RoadSpeed = PVehicle->Wheels.IsValidIndex(WheelIdx) ? FMath::Abs(PVehicle->Wheels[WheelIdx].GetRoadSpeed()) : 0.0f;
		const float MaxRise = FMath::Max(SinkOffsetMinRiseSpeed, RoadSpeed * SinkOffsetRiseSlope) * LastDeltaTime;
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

void FB1VehicleSimulation::ApplyWheelFrictionForces(float DeltaTime)
{
	const int32 NumWheels = PVehicle->Wheels.Num();
	UpdateRelativeLoads();
	MudFrictionScale.Init(1.0f, NumWheels);

	// The base call sets SurfaceFriction from the material itself, so the depth curve goes on the multiplier.
	TArray<float, TInlineAllocator<8>> SavedMultiplier;
	SavedMultiplier.SetNumUninitialized(NumWheels);
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		Chaos::FSimpleWheelSim& PWheel = PVehicle->Wheels[WheelIdx];
		SavedMultiplier[WheelIdx] = PWheel.FrictionMultiplier;

		if (const FB1MudSimParams* Mud = FindContactMud(WheelIdx))
		{
			MudFrictionScale[WheelIdx] = FMath::Max(Mud->FrictionVsDepth.Eval(GetNormalizedDepth(WheelIdx, *Mud), 1.0f), 0.0f);
			PWheel.FrictionMultiplier *= MudFrictionScale[WheelIdx];
		}
	}

	UChaosWheeledVehicleSimulation::ApplyWheelFrictionForces(DeltaTime);

	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		PVehicle->Wheels[WheelIdx].FrictionMultiplier = SavedMultiplier[WheelIdx];
	}

	ApplyRollingResistance(DeltaTime);
	ApplyBellyDrag(DeltaTime);
}

void FB1VehicleSimulation::ApplyRollingResistance(float DeltaTime)
{
	const int32 NumWheels = PVehicle->Wheels.Num();
	RollingResistance.Init(0.0f, NumWheels);
	if (NumWheelsInContact == 0 || DeltaTime <= 0.0f)
	{
		return;
	}

	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const FB1MudSimParams* Mud = FindContactMud(WheelIdx);
		if (!Mud)
		{
			continue;
		}

		// Chaos sets MassPerWheel to vehicle mass / wheel count for wheels in contact.
		const Chaos::FSimpleWheelSim& PWheel = PVehicle->Wheels[WheelIdx];
		const float Mass = PWheel.MassPerWheel * NumWheels;
		const float WeightShare = Mass * GravityCmPerS2 / NumWheelsInContact * RelativeLoad[WheelIdx];
		const float RoadSpeed = PWheel.GetRoadSpeed();
		const float NormalizedDepth = GetNormalizedDepth(WheelIdx, *Mud);
		const float SpeedMps = RoadSpeed / CmPerM;
		const float Coefficient = FMath::Max(Mud->RollingResistanceVsDepth.Eval(NormalizedDepth, 0.0f), 0.0f)
			+ Mud->SpeedDragCoefficient * NormalizedDepth * SpeedMps * SpeedMps;

		// Opposes motion, or at standstill the tire's push, like static friction, so a spinning wheel can't creep.
		// Capped at the tire's push plus a share of the stopping force, so it never reverses the wheel.
		const float TireForce = PWheel.GetForceFromFriction().X;
		const float MotionSign = FMath::Abs(RoadSpeed) > StandstillSpeed ? FMath::Sign(RoadSpeed) : FMath::Sign(TireForce);
		const float TirePush = FMath::Max(TireForce * MotionSign, 0.0f);
		const float StopForce = PWheel.MassPerWheel * FMath::Abs(RoadSpeed) / DeltaTime * MaxStopForceFraction;
		const float Force = FMath::Min(Coefficient * WeightShare, TirePush + StopForce);
		if (Force <= 0.0f || MotionSign == 0.0f)
		{
			continue;
		}

		const FHitResult& Hit = WheelState.TraceResult[WheelIdx];
		const FVector GroundForward = FVector::CrossProduct(VehicleState.VehicleRightAxis, Hit.Normal).GetSafeNormal();
		const FVector WheelForward = GroundForward.RotateAngleAxis(PWheel.SteeringAngle, Hit.Normal);

		AddForceAtPosition(WheelForward * -MotionSign * Force, Hit.ImpactPoint);
		RollingResistance[WheelIdx] = Force / NewtonsToUnreal;
	}
}

void FB1VehicleSimulation::ApplyBellyDrag(float DeltaTime)
{
	BellyDepth = 0.0f;
	BellyDrag = 0.0f;
	const int32 NumWheels = PVehicle->Wheels.Num();
	if (NumWheels == 0 || NumWheelsInContact == 0 || DeltaTime <= 0.0f)
	{
		return;
	}

	float DepthSum = 0.0f;
	float CoefficientSum = 0.0f;
	int32 NumMudWheels = 0;
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		DepthSum += SinkDepth.IsValidIndex(WheelIdx) ? SinkDepth[WheelIdx] : 0.0f;
		if (const FB1MudSimParams* Mud = FindContactMud(WheelIdx))
		{
			CoefficientSum += Mud->BellyDragCoefficient;
			++NumMudWheels;
		}
	}

	BellyDepth = FMath::Max(DepthSum / NumWheels - StepInput.BellyClearance, 0.0f);
	if (BellyDepth <= 0.0f || NumMudWheels == 0)
	{
		return;
	}

	const float Mass = PVehicle->Wheels[0].MassPerWheel * NumWheels;
	const float DragLimit = CoefficientSum / NumMudWheels * BellyDepth / CmPerM * Mass * GravityCmPerS2;

	// Same rules as rolling resistance: opposes ground motion, or at standstill the tires' push, and never reverses it.
	const FVector Up = VehicleState.VehicleUpAxis;
	const FVector GroundVelocity = FVector::VectorPlaneProject(VehicleState.VehicleWorldVelocity, Up);
	const float Speed = GroundVelocity.Size();

	FVector TirePush = FVector::ZeroVector;
	for (const Chaos::FSimpleWheelSim& PWheel : PVehicle->Wheels)
	{
		TirePush += VehicleState.VehicleForwardAxis * PWheel.GetForceFromFriction().X;
	}

	const FVector Direction = Speed > StandstillSpeed ? GroundVelocity / Speed : TirePush.GetSafeNormal();
	const float PushAlongMotion = FMath::Max(FVector::DotProduct(TirePush, Direction), 0.0f);
	const float StopForce = Mass * Speed / DeltaTime * MaxStopForceFraction;
	const float Force = FMath::Min(DragLimit, PushAlongMotion + StopForce);
	if (Force <= 0.0f || Direction.IsNearlyZero())
	{
		return;
	}

	AddForce(-Direction * Force);
	BellyDrag = Force / NewtonsToUnreal;
}

void FB1VehicleSimulation::UpdateRelativeLoads()
{
	const int32 NumWheels = PVehicle->Wheels.Num();

	// Chaos's reported load doesn't match RestingForce at rest, so loads are relative to the average.
	float LoadSum = 0.0f;
	NumWheelsInContact = 0;
	for (const Chaos::FSimpleWheelSim& PWheel : PVehicle->Wheels)
	{
		if (PWheel.InContact())
		{
			LoadSum += PWheel.GetWheelLoadForce();
			++NumWheelsInContact;
		}
	}
	const float AverageLoad = NumWheelsInContact > 0 ? LoadSum / NumWheelsInContact : 0.0f;

	RelativeLoad.SetNumUninitialized(NumWheels);
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const float Load = PVehicle->Wheels[WheelIdx].GetWheelLoadForce();
		RelativeLoad[WheelIdx] = AverageLoad > 0.0f ? FMath::Clamp(Load / AverageLoad, 0.0f, MaxLoadFactor) : 1.0f;
	}
}

const FB1MudSimParams* FB1VehicleSimulation::FindContactMud(int32 WheelIdx) const
{
	if (!PVehicle->Wheels[WheelIdx].InContact())
	{
		return nullptr;
	}

	return FindMud(WheelState.TraceResult[WheelIdx].PhysMaterial);
}

const FB1MudSimParams* FB1VehicleSimulation::FindMud(const TWeakObjectPtr<UPhysicalMaterial>& Material) const
{
	return StepInput.MudMaterials.FindByPredicate([&Material](const FB1MudSimParams& Params)
	{
		return Params.Material == Material;
	});
}

float FB1VehicleSimulation::TraceHardGround(const FVector& SurfacePoint, const FVector& Down, float MaxDepth, const FCollisionQueryParams& TraceParams, const FCollisionResponseParams& ResponseParams) const
{
	// Starts a little above the surface, so a ramp that meets the surface still counts as depth 0.
	const FVector Start = SurfacePoint - Down * HardGroundTraceLift;
	const FVector End = SurfacePoint + Down * MaxDepth;

	FHitResult GroundHit;
	if (World && World->LineTraceSingleByChannel(GroundHit, Start, End, ECC_WorldDynamic, TraceParams, ResponseParams))
	{
		return FMath::Max(GroundHit.Distance - HardGroundTraceLift, 0.0f);
	}
	return MAX_flt;
}

float FB1VehicleSimulation::GetNormalizedDepth(int32 WheelIdx, const FB1MudSimParams& Mud) const
{
	const float Depth = SinkDepth.IsValidIndex(WheelIdx) ? SinkDepth[WheelIdx] : 0.0f;
	return Mud.MaxSinkDepth > 0.0f ? FMath::Clamp(Depth / Mud.MaxSinkDepth, 0.0f, 1.0f) : 0.0f;
}

void FB1VehicleSimulation::UpdateSinkDepth(float DeltaTime)
{
	const int32 NumWheels = PVehicle->Wheels.Num();
	SinkDepth.SetNumZeroed(NumWheels);

	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const Chaos::FSimpleWheelSim& PWheel = PVehicle->Wheels[WheelIdx];

		// An airborne wheel keeps its depth, so a bounce in the mud doesn't reset it.
		if (!PWheel.InContact())
		{
			continue;
		}

		const FB1MudSimParams* Mud = FindContactMud(WheelIdx);
		if (!Mud)
		{
			SinkDepth[WheelIdx] = 0.0f;
			continue;
		}

		// Weight sets a static floor; wheelspin digs below it, rolling with grip climbs back to it.
		const float Load = RelativeLoad.IsValidIndex(WheelIdx) ? RelativeLoad[WheelIdx] : 1.0f;
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
		const float HardGround = HardGroundDepth.IsValidIndex(WheelIdx) ? HardGroundDepth[WheelIdx] : MAX_flt;
		SinkDepth[WheelIdx] = FMath::Clamp(Depth, 0.0f, FMath::Min(Mud->MaxSinkDepth, HardGround));
	}
}
