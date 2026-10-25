#include "B1VehicleMovementComponent.h"
#include "B1VehicleSimulation.h"
#include "B1Log.h"
#include "DrawDebugHelpers.h"

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

void UB1VehicleMovementComponent::Update(float DeltaTime)
{
	Super::Update(DeltaTime);

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

FB1VehicleSimulation* UB1VehicleMovementComponent::GetB1Simulation() const
{
	// CreatePhysicsVehicle always creates an FB1VehicleSimulation.
	return static_cast<FB1VehicleSimulation*>(VehicleSimulationPT.Get());
}
