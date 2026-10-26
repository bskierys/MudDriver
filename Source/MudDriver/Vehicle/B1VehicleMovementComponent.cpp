#include "B1VehicleMovementComponent.h"
#include "B1VehicleSimulation.h"
#include "B1Log.h"
#include "B1CollisionChannels.h"
#include "B1PhysicalMaterial_Mud.h"
#include "DrawDebugHelpers.h"

UB1VehicleMovementComponent::UB1VehicleMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WheelTraceCollisionResponses.SetResponse(B1_ObjectChannel_Mud, ECR_Block);
}

TUniquePtr<Chaos::FSimpleWheeledVehicle> UB1VehicleMovementComponent::CreatePhysicsVehicle()
{
	// Mirrors UChaosWheeledVehicleMovementComponent::CreatePhysicsVehicle, which hardcodes its simulation class.
	VehicleSimulationPT = MakeUnique<FB1VehicleSimulation>();

	return UChaosVehicleMovementComponent::CreatePhysicsVehicle();
}

void UB1VehicleMovementComponent::SetTestForce(EB1TestForce Mode)
{
	if (TestForce == EB1TestForce::Off && Mode != EB1TestForce::Off)
	{
		SavedSleepThreshold = SleepThreshold;
		SleepThreshold = 0.0f;
		SetSleeping(false);
	}
	else if (TestForce != EB1TestForce::Off && Mode == EB1TestForce::Off)
	{
		SleepThreshold = SavedSleepThreshold;
	}

	TestForce = Mode;
	TestForceAnchor = GetOwner()->GetActorLocation();

	UE_LOG(LogB1Physics, Log, TEXT("TestForce %s on %s, anchor %s"),
		*UEnum::GetValueAsString(Mode), *GetNameSafe(GetOwner()), *TestForceAnchor.ToCompactString());
}

float UB1VehicleMovementComponent::GetAverageSinkDepth() const
{
	const int32 NumWheels = GetNumWheels();
	float Sum = 0.0f;
	for (int32 WheelIndex = 0; WheelIndex < NumWheels; ++WheelIndex)
	{
		Sum += GetWheelSinkDepth(WheelIndex);
	}
	return NumWheels > 0 ? Sum / NumWheels : 0.0f;
}

void UB1VehicleMovementComponent::UpdateEnginePower()
{
	if (PeakEnginePower <= 0.0f)
	{
		constexpr int32 NumSamples = 64;
		for (int32 Sample = 1; Sample <= NumSamples; ++Sample)
		{
			const float RPM = EngineSetup.MaxRPM * Sample / NumSamples;
			PeakEnginePower = FMath::Max(PeakEnginePower, EngineSetup.GetTorqueFromRPM(RPM) * RPM);
		}
	}

	// Same throttle the physics engine gets: the rate config's control function, squared by the wheeled sim.
	EffectiveThrottle = ThrottleInputRate.CalcControlFunction(ThrottleInput);
	const float RPM = GetEngineRotationSpeed();
	const float Power = EffectiveThrottle * EffectiveThrottle * EngineSetup.GetTorqueFromRPM(RPM) * RPM;
	NormalizedEnginePower = PeakEnginePower > 0.0f ? FMath::Clamp(Power / PeakEnginePower, 0.0f, 1.0f) : 0.0f;
}

void UB1VehicleMovementComponent::Update(float DeltaTime)
{
	// Chaos engine torque is throttle × curve(RPM), so a zero throttle also removes idle torque.
	if (bFuelCutOff)
	{
		ThrottleInput = 0.0f;
	}

	Super::Update(DeltaTime);

	UpdateEnginePower();

	FB1VehicleSimulation* Simulation = GetB1Simulation();
	if (!CurAsyncInput || !Simulation)
	{
		return;
	}

	FB1VehicleSimInput Input;
	Input.TestForce = TestForce;
	Input.TestForceSettings = TestForceSettings;
	Input.TestForceAnchor = TestForceAnchor;
	GatherMudParams(Input.MudMaterials);
	Input.BellyClearance = BellyClearance;
	Simulation->SetGameThreadInput(Input);

	SimOutput = Simulation->GetGameThreadOutput();

	if (!bLoggedClearance)
	{
		LogMeasuredClearance();
	}

#if ENABLE_DRAW_DEBUG
	if (TestForce == EB1TestForce::Spring)
	{
		const FVector Location = GetOwner()->GetActorLocation();
		const bool bTaut = FVector::Dist(Location, TestForceAnchor) > TestForceSettings.SpringRestLength * 100.0f;
		DrawDebugLine(GetWorld(), Location, TestForceAnchor, bTaut ? FColor::Red : FColor::Green, false, -1.0f, 0, 3.0f);
	}
#endif
}

float UB1VehicleMovementComponent::GetWheelSinkDepth(int32 WheelIndex) const
{
	return SimOutput.SinkDepth.IsValidIndex(WheelIndex) ? SimOutput.SinkDepth[WheelIndex] : 0.0f;
}

float UB1VehicleMovementComponent::GetWheelMudFrictionScale(int32 WheelIndex) const
{
	return SimOutput.MudFrictionScale.IsValidIndex(WheelIndex) ? SimOutput.MudFrictionScale[WheelIndex] : 1.0f;
}

float UB1VehicleMovementComponent::GetWheelRollingResistance(int32 WheelIndex) const
{
	return SimOutput.RollingResistance.IsValidIndex(WheelIndex) ? SimOutput.RollingResistance[WheelIndex] : 0.0f;
}

void UB1VehicleMovementComponent::GatherMudParams(TArray<FB1MudSimParams>& OutParams)
{
	for (int32 WheelIndex = 0; WheelIndex < GetNumWheels(); ++WheelIndex)
	{
		if (UB1PhysicalMaterial_Mud* Mud = Cast<UB1PhysicalMaterial_Mud>(GetWheelState(WheelIndex).PhysMaterial.Get()))
		{
			KnownMudMaterials.AddUnique(Mud);
		}
	}

	OutParams.Reset(KnownMudMaterials.Num());
	for (UB1PhysicalMaterial_Mud* Mud : KnownMudMaterials)
	{
		FB1MudSimParams& Params = OutParams.AddDefaulted_GetRef();
		Params.Material = Mud;
		Params.MaxSinkDepth = Mud->MaxSinkDepth;
		Params.StaticSinkDepth = Mud->MaxSinkDepth * Mud->StaticSinkRatio;
		Params.SettleRate = Mud->SettleRate;
		Params.SinkRate = Mud->SinkRate;
		Params.RiseRate = Mud->RiseRate;
		Params.FullSinkSlipSpeed = Mud->FullSinkSlipSpeed;
		Params.FrictionVsDepth = *Mud->FrictionVsDepth.GetRichCurveConst();
		Params.RollingResistanceVsDepth = *Mud->RollingResistanceVsDepth.GetRichCurveConst();
		Params.SpeedDragCoefficient = Mud->SpeedDragCoefficient;
		Params.BellyDragCoefficient = Mud->BellyDragCoefficient;
	}
}

void UB1VehicleMovementComponent::LogMeasuredClearance()
{
	const FBodyInstance* Body = UpdatedPrimitive ? UpdatedPrimitive->GetBodyInstance() : nullptr;
	if (!Body || GetNumWheels() == 0 || GetForwardSpeed() > 10.0f)
	{
		return;
	}

	float ContactZ = 0.0f;
	for (int32 WheelIndex = 0; WheelIndex < GetNumWheels(); ++WheelIndex)
	{
		const FWheelStatus& Wheel = GetWheelState(WheelIndex);
		if (!Wheel.bInContact || Cast<UB1PhysicalMaterial_Mud>(Wheel.PhysMaterial.Get()))
		{
			return;
		}
		ContactZ += Wheel.HitLocation.Z / GetNumWheels();
	}

	bLoggedClearance = true;
	UE_LOG(LogB1Physics, Log, TEXT("%s belly clearance: measured %.1f cm, BellyClearance %.1f cm"),
		*GetNameSafe(GetOwner()), Body->GetBodyBounds().Min.Z - ContactZ, BellyClearance);
}

FB1VehicleSimulation* UB1VehicleMovementComponent::GetB1Simulation() const
{
	// CreatePhysicsVehicle always creates an FB1VehicleSimulation.
	return static_cast<FB1VehicleSimulation*>(VehicleSimulationPT.Get());
}
