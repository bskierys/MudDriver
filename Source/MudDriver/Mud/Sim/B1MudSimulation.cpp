#include "B1MudSimulation.h"
#include "B1MudSettings.h"
#include "Engine/World.h"

namespace
{
	constexpr float HardGroundTraceLift = 2.0f;
}

FB1MudSimulation::FB1MudSimulation()
	: MudChannel(GetDefault<UB1MudSettings>()->MudObjectChannel)
{
}

void FB1MudSimulation::SetGameThreadInput(const FB1MudSimInput& Input)
{
	FScopeLock Lock(&InputLock);
	PendingInput = Input;
}

FB1MudSimOutput FB1MudSimulation::GetGameThreadOutput() const
{
	FScopeLock Lock(&OutputLock);
	return LatestOutput;
}

void FB1MudSimulation::BeginStep()
{
	{
		FScopeLock Lock(&InputLock);
		StepInput = PendingInput;
	}
	ExternalForce = FVector::ZeroVector;

	if (StepInput.SinkResetSerial != AppliedSinkResetSerial)
	{
		AppliedSinkResetSerial = StepInput.SinkResetSerial;
		SinkDepth.Init(0.0f, SinkDepth.Num());
		SinkOffset.Init(0.0f, SinkOffset.Num());
	}
}

void FB1MudSimulation::EndStep(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime)
{
	LastDeltaTime = DeltaTime;
	if (!StepInput.UnstuckAssistForce.IsZero())
	{
		Vehicle.AddForce(StepInput.UnstuckAssistForce * B1MudWheelModel::NewtonsToUnreal);
	}
	UpdateSinkDepth(Vehicle, DeltaTime);

	TreadSpeed.SetNumUninitialized(Vehicle.PVehicle->Wheels.Num());
	for (int32 WheelIdx = 0; WheelIdx < TreadSpeed.Num(); ++WheelIdx)
	{
		TreadSpeed[WheelIdx] = FMath::Abs(Vehicle.PVehicle->Wheels[WheelIdx].GetWheelGroundSpeed());
	}

	FScopeLock Lock(&OutputLock);
	LatestOutput.TreadSpeed = TreadSpeed;
	LatestOutput.SinkDepth = SinkDepth;
	LatestOutput.MudFrictionScale = MudFrictionScale;
	LatestOutput.RollingResistance = RollingResistance;
	LatestOutput.SlopePull = SlopePull;
	LatestOutput.NetPush = WheelNetPush;
	LatestOutput.BellyDepth = BellyDepth;
	LatestOutput.BellyDrag = BellyDrag;
	LatestOutput.MudGroundUpZ = MudGroundUpZ;
	LatestOutput.SinkResetSerial = AppliedSinkResetSerial;
}

void FB1MudSimulation::PreSuspensionTraces(const UChaosWheeledVehicleSimulation& Vehicle, TArray<Chaos::FSuspensionTrace>& Traces)
{
	// SinkDepth is from the previous step; traces run before EndStep.
	const int32 NumWheels = Traces.Num();
	SinkOffset.SetNumZeroed(NumWheels);
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const float RoadSpeed = Vehicle.PVehicle->Wheels.IsValidIndex(WheelIdx) ? FMath::Abs(Vehicle.PVehicle->Wheels[WheelIdx].GetRoadSpeed()) : 0.0f;
		SinkOffset[WheelIdx] = B1MudWheelModel::UpdateSinkOffset(SinkOffset[WheelIdx], GetSinkDepth(WheelIdx), RoadSpeed, LastDeltaTime);
	}

	// A sunk chassis drags the trace start down into the thin mud slab, so trace from a start raised by the
	// offset. Chaos reads Distance as measured from the real start, which makes the hit exactly Offset deeper.
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		Chaos::FSuspensionTrace& Trace = Traces[WheelIdx];
		Trace.Start -= (Trace.End - Trace.Start).GetSafeNormal() * SinkOffset[WheelIdx];
	}
}

void FB1MudSimulation::PostSuspensionTraces(UChaosWheeledVehicleSimulation& Vehicle, const TArray<Chaos::FSuspensionTrace>& RaisedTraces, const FCollisionQueryParams& TraceParams, const FCollisionResponseContainer& CollisionResponse)
{
	HardGroundDepth.Init(MAX_flt, RaisedTraces.Num());
	FCollisionResponseParams GroundResponse(CollisionResponse);
	GroundResponse.CollisionResponse.SetResponse(MudChannel, ECR_Ignore);

	for (int32 WheelIdx = 0; WheelIdx < RaisedTraces.Num(); ++WheelIdx)
	{
		FHitResult& Hit = Vehicle.WheelState.TraceResult[WheelIdx];
		if (!Hit.bBlockingHit)
		{
			continue;
		}

		const FVector Down = (RaisedTraces[WheelIdx].End - RaisedTraces[WheelIdx].Start).GetSafeNormal();
		const float RaisedBy = SinkOffset[WheelIdx];
		const float SurfaceDistance = Hit.Distance - RaisedBy;

		// Where the bottom ramps up (or the landscape is shallow) the wheel can't sink past it.
		if (const FB1MudSimParams* Mud = FindMud(Hit.PhysMaterial))
		{
			HardGroundDepth[WheelIdx] = TraceHardGround(Vehicle.World, Hit.ImpactPoint, Down, Mud->MaxSinkDepth, TraceParams, GroundResponse);
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

void FB1MudSimulation::PreFriction(UChaosWheeledVehicleSimulation& Vehicle)
{
	const int32 NumWheels = Vehicle.PVehicle->Wheels.Num();
	UpdateRelativeLoads(Vehicle);
	MudFrictionScale.Init(1.0f, NumWheels);

	// Chaos brakes with EngineRPM × EngineBrakeEffect at zero throttle, and its brake locks a near-stopped wheel.
	// On a muddy slope with no input the wheels roll instead, so the slope hold decides whether the jeep slides.
	const FControlInputs& Inputs = Vehicle.VehicleInputs;
	const bool bNoInput = Inputs.ThrottleInput < SMALL_NUMBER && Inputs.BrakeInput < SMALL_NUMBER && Inputs.HandbrakeInput < SMALL_NUMBER && !Inputs.ParkingEnabled;

	// The base call sets SurfaceFriction from the material itself, so the depth curve goes on the multiplier.
	SavedFrictionMultiplier.SetNumUninitialized(NumWheels);
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		Chaos::FSimpleWheelSim& PWheel = Vehicle.PVehicle->Wheels[WheelIdx];
		SavedFrictionMultiplier[WheelIdx] = PWheel.FrictionMultiplier;

		if (const FB1MudSimParams* Mud = FindContactMud(Vehicle, WheelIdx))
		{
			MudFrictionScale[WheelIdx] = B1MudWheelModel::GetFrictionScale(GetSinkDepth(WheelIdx), *Mud);
			PWheel.FrictionMultiplier *= MudFrictionScale[WheelIdx];

			if (bNoInput && Vehicle.WheelState.TraceResult[WheelIdx].Normal.Z < StepInput.SlopeUpZ)
			{
				PWheel.SetBrakeTorque(0.0f);
			}
		}
	}
}

void FB1MudSimulation::PostFriction(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime)
{
	for (int32 WheelIdx = 0; WheelIdx < Vehicle.PVehicle->Wheels.Num() && WheelIdx < SavedFrictionMultiplier.Num(); ++WheelIdx)
	{
		Vehicle.PVehicle->Wheels[WheelIdx].FrictionMultiplier = SavedFrictionMultiplier[WheelIdx];
	}

	ApplyRollingResistance(Vehicle, DeltaTime);
	ApplyBellyDrag(Vehicle, DeltaTime);
}

void FB1MudSimulation::ApplyRollingResistance(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime)
{
	const int32 NumWheels = Vehicle.PVehicle->Wheels.Num();
	RollingResistance.Init(0.0f, NumWheels);
	SlopePull.Init(0.0f, NumWheels);
	WheelNetPush.Init(0.0f, NumWheels);
	if (NumWheelsInContact == 0 || DeltaTime <= 0.0f)
	{
		return;
	}

	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const FB1MudSimParams* Mud = FindContactMud(Vehicle, WheelIdx);
		if (!Mud)
		{
			continue;
		}

		// Chaos sets MassPerWheel to vehicle mass / wheel count for wheels in contact.
		const Chaos::FSimpleWheelSim& PWheel = Vehicle.PVehicle->Wheels[WheelIdx];
		const float Mass = PWheel.MassPerWheel * NumWheels;
		const float WeightShare = Mass * B1MudWheelModel::GravityCmPerS2 / NumWheelsInContact * RelativeLoad[WheelIdx];
		const float RoadSpeed = PWheel.GetRoadSpeed();
		const float Coefficient = B1MudWheelModel::GetRollingResistanceCoefficient(GetSinkDepth(WheelIdx), *Mud, RoadSpeed / B1MudWheelModel::CmPerM);

		const FHitResult& Hit = Vehicle.WheelState.TraceResult[WheelIdx];
		const FVector GroundForward = FVector::CrossProduct(Vehicle.VehicleState.VehicleRightAxis, Hit.Normal).GetSafeNormal();
		const FVector WheelForward = GroundForward.RotateAngleAxis(PWheel.SteeringAngle, Hit.Normal);

		// Net = tire force + gravity along the ground: on a slope the tire force is mostly holding the jeep, not pushing it.
		float NetPush = PWheel.GetForceFromFriction().X - WeightShare * WheelForward.Z;

		// The host's external force (winch) is shared like the weight, so all mud wheels together see all of it.
		const float ExternalShare = RelativeLoad[WheelIdx] / NumWheelsInContact;
		NetPush += FVector::DotProduct(ExternalForce, WheelForward) * B1MudWheelModel::NewtonsToUnreal * ExternalShare;

		// Mud on a slope is slippery: an extra pull down the slope, always on, so climbing is hard and a stopped jeep slides.
		if (Hit.Normal.Z < StepInput.SlopeUpZ)
		{
			const FVector Pull = B1MudWheelModel::GetSlopePull(Hit.Normal, WeightShare, StepInput.SlopePull);
			Vehicle.AddForceAtPosition(Pull, Hit.ImpactPoint);
			NetPush += FVector::DotProduct(Pull, WheelForward);
			SlopePull[WheelIdx] = Pull.Size() / B1MudWheelModel::NewtonsToUnreal;
		}

		WheelNetPush[WheelIdx] = NetPush / B1MudWheelModel::NewtonsToUnreal;

		// Opposes motion, or at standstill the net push, like static friction, so a spinning wheel can't creep.
		const float MotionSign = FMath::Abs(RoadSpeed) > B1MudWheelModel::StandstillSpeed ? FMath::Sign(RoadSpeed) : FMath::Sign(NetPush);
		const float PushAlongMotion = FMath::Max(NetPush * MotionSign, 0.0f);
		const float StopForce = B1MudWheelModel::GetStopForce(PWheel.MassPerWheel, FMath::Abs(RoadSpeed), DeltaTime);
		const float Force = B1MudWheelModel::LimitResistance(Coefficient * WeightShare, PushAlongMotion, StopForce);
		if (Force <= 0.0f || MotionSign == 0.0f)
		{
			continue;
		}

		Vehicle.AddForceAtPosition(WheelForward * -MotionSign * Force, Hit.ImpactPoint);
		RollingResistance[WheelIdx] = Force / B1MudWheelModel::NewtonsToUnreal;
	}
}

void FB1MudSimulation::ApplyBellyDrag(UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime)
{
	BellyDepth = 0.0f;
	BellyDrag = 0.0f;
	const int32 NumWheels = Vehicle.PVehicle->Wheels.Num();
	if (NumWheels == 0 || NumWheelsInContact == 0 || DeltaTime <= 0.0f)
	{
		return;
	}

	float DepthSum = 0.0f;
	float CoefficientSum = 0.0f;
	int32 NumMudWheels = 0;
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		DepthSum += GetSinkDepth(WheelIdx);
		if (const FB1MudSimParams* Mud = FindContactMud(Vehicle, WheelIdx))
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

	const float Mass = Vehicle.PVehicle->Wheels[0].MassPerWheel * NumWheels;
	const float DragLimit = B1MudWheelModel::GetBellyDragLimit(CoefficientSum / NumMudWheels, BellyDepth, Mass);

	// Same rules as rolling resistance: opposes ground motion, or at standstill the tires' push, and never reverses it.
	const FVehicleState& State = Vehicle.VehicleState;
	const FVector GroundVelocity = FVector::VectorPlaneProject(State.VehicleWorldVelocity, State.VehicleUpAxis);
	const float Speed = GroundVelocity.Size();

	// Net push: tires, gravity and the external force on the ground plane, as for rolling resistance.
	FVector NetPush = FVector::ZeroVector;
	FVector GroundUp = FVector::ZeroVector;
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const Chaos::FSimpleWheelSim& PWheel = Vehicle.PVehicle->Wheels[WheelIdx];
		NetPush += State.VehicleForwardAxis * PWheel.GetForceFromFriction().X;
		if (PWheel.InContact())
		{
			GroundUp += Vehicle.WheelState.TraceResult[WheelIdx].Normal;
		}
	}
	NetPush += FVector::VectorPlaneProject(FVector(0.0f, 0.0f, -Mass * B1MudWheelModel::GravityCmPerS2), GroundUp.GetSafeNormal());
	NetPush += FVector::VectorPlaneProject(ExternalForce * B1MudWheelModel::NewtonsToUnreal, GroundUp.GetSafeNormal());

	const FVector Direction = Speed > B1MudWheelModel::StandstillSpeed ? GroundVelocity / Speed : NetPush.GetSafeNormal();
	const float PushAlongMotion = FMath::Max(FVector::DotProduct(NetPush, Direction), 0.0f);
	const float Force = B1MudWheelModel::LimitResistance(DragLimit, PushAlongMotion, B1MudWheelModel::GetStopForce(Mass, Speed, DeltaTime));
	if (Force <= 0.0f || Direction.IsNearlyZero())
	{
		return;
	}

	Vehicle.AddForce(-Direction * Force);
	BellyDrag = Force / B1MudWheelModel::NewtonsToUnreal;
}

void FB1MudSimulation::UpdateRelativeLoads(const UChaosWheeledVehicleSimulation& Vehicle)
{
	const int32 NumWheels = Vehicle.PVehicle->Wheels.Num();

	// Chaos's reported load doesn't match RestingForce at rest, so loads are relative to the average.
	float LoadSum = 0.0f;
	NumWheelsInContact = 0;
	for (const Chaos::FSimpleWheelSim& PWheel : Vehicle.PVehicle->Wheels)
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
		RelativeLoad[WheelIdx] = B1MudWheelModel::GetRelativeLoad(Vehicle.PVehicle->Wheels[WheelIdx].GetWheelLoadForce(), AverageLoad);
	}
}

void FB1MudSimulation::UpdateSinkDepth(const UChaosWheeledVehicleSimulation& Vehicle, float DeltaTime)
{
	const int32 NumWheels = Vehicle.PVehicle->Wheels.Num();
	SinkDepth.SetNumZeroed(NumWheels);
	MudGroundUpZ = 1.0f;

	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		const Chaos::FSimpleWheelSim& PWheel = Vehicle.PVehicle->Wheels[WheelIdx];

		// An airborne wheel keeps its depth, so a bounce in the mud doesn't reset it.
		if (!PWheel.InContact())
		{
			continue;
		}

		const FB1MudSimParams* Mud = FindContactMud(Vehicle, WheelIdx);
		if (!Mud)
		{
			SinkDepth[WheelIdx] = 0.0f;
			continue;
		}
		MudGroundUpZ = FMath::Min(MudGroundUpZ, static_cast<float>(Vehicle.WheelState.TraceResult[WheelIdx].Normal.Z));

		FB1MudSinkInput Input;
		Input.SlipSpeed = FMath::Abs(PWheel.GetSlipMagnitude()) / B1MudWheelModel::CmPerM;
		Input.RoadSpeed = FMath::Abs(PWheel.GetRoadSpeed()) / B1MudWheelModel::CmPerM;
		Input.RelativeLoad = RelativeLoad.IsValidIndex(WheelIdx) ? RelativeLoad[WheelIdx] : 1.0f;
		Input.HardGroundDepth = HardGroundDepth.IsValidIndex(WheelIdx) ? HardGroundDepth[WheelIdx] : MAX_flt;
		Input.DeltaTime = DeltaTime;
		SinkDepth[WheelIdx] = B1MudWheelModel::UpdateSinkDepth(SinkDepth[WheelIdx], *Mud, Input);
	}
}

const FB1MudSimParams* FB1MudSimulation::FindContactMud(const UChaosWheeledVehicleSimulation& Vehicle, int32 WheelIdx) const
{
	if (!Vehicle.PVehicle->Wheels[WheelIdx].InContact())
	{
		return nullptr;
	}

	return FindMud(Vehicle.WheelState.TraceResult[WheelIdx].PhysMaterial);
}

const FB1MudSimParams* FB1MudSimulation::FindMud(const TWeakObjectPtr<UPhysicalMaterial>& Material) const
{
	return StepInput.MudMaterials.FindByPredicate([&Material](const FB1MudSimParams& Params)
	{
		return Params.Material == Material;
	});
}

float FB1MudSimulation::TraceHardGround(const UWorld* World, const FVector& SurfacePoint, const FVector& Down, float MaxDepth, const FCollisionQueryParams& TraceParams, const FCollisionResponseParams& ResponseParams) const
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
