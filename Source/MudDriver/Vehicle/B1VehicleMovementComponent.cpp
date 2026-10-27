#include "B1VehicleMovementComponent.h"
#include "B1VehicleSimulation.h"
#include "B1Log.h"
#include "B1MudSettings.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosVehicleWheel.h"

UB1VehicleMovementComponent::UB1VehicleMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UB1VehicleMovementComponent::OnRegister()
{
	Super::OnRegister();

	// Here, not in the constructor: the CDO can be built before the settings are read from config.
	WheelTraceCollisionResponses.SetResponse(GetDefault<UB1MudSettings>()->MudObjectChannel, ECR_Block);
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
	Simulation->SetGameThreadInput(Input);

#if ENABLE_DRAW_DEBUG
	if (TestForce == EB1TestForce::Spring)
	{
		const FVector Location = GetOwner()->GetActorLocation();
		const bool bTaut = FVector::Dist(Location, TestForceAnchor) > TestForceSettings.SpringRestLength * 100.0f;
		DrawDebugLine(GetWorld(), Location, TestForceAnchor, bTaut ? FColor::Red : FColor::Green, false, -1.0f, 0, 3.0f);
	}
#endif
}

FB1MudSimulation* UB1VehicleMovementComponent::GetMudSimulation() const
{
	FB1VehicleSimulation* Simulation = GetB1Simulation();
	return Simulation ? &Simulation->GetMudSimulation() : nullptr;
}

FVector UB1VehicleMovementComponent::GetMudWheelLocation(int32 WheelIndex) const
{
	const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(UpdatedPrimitive);
	return Mesh && WheelSetups.IsValidIndex(WheelIndex) ? Mesh->GetSocketLocation(WheelSetups[WheelIndex].BoneName) : GetOwner()->GetActorLocation();
}

float UB1VehicleMovementComponent::GetMudWheelRadius(int32 WheelIndex) const
{
	return Wheels.IsValidIndex(WheelIndex) && Wheels[WheelIndex] ? Wheels[WheelIndex]->GetWheelRadius() : 0.0f;
}

float UB1VehicleMovementComponent::GetMudWheelWidth(int32 WheelIndex) const
{
	return Wheels.IsValidIndex(WheelIndex) && Wheels[WheelIndex] ? Wheels[WheelIndex]->WheelWidth : 0.0f;
}

float UB1VehicleMovementComponent::GetMudDriveInput() const
{
	return FMath::Clamp(RawThrottleInput - RawBrakeInput, -1.0f, 1.0f);
}

FB1VehicleSimulation* UB1VehicleMovementComponent::GetB1Simulation() const
{
	// CreatePhysicsVehicle always creates an FB1VehicleSimulation.
	return static_cast<FB1VehicleSimulation*>(VehicleSimulationPT.Get());
}
