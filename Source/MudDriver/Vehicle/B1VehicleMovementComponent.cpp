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
	GatherMudParams(Input.MudMaterials);
	Simulation->SetGameThreadInput(Input);

	SimOutput = Simulation->GetGameThreadOutput();

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
		Params.SinkRate = Mud->SinkRate;
		Params.RiseRate = Mud->RiseRate;
		Params.FullSinkSlipSpeed = Mud->FullSinkSlipSpeed;
	}
}

FB1VehicleSimulation* UB1VehicleMovementComponent::GetB1Simulation() const
{
	// CreatePhysicsVehicle always creates an FB1VehicleSimulation.
	return static_cast<FB1VehicleSimulation*>(VehicleSimulationPT.Get());
}
