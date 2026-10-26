#include "B1MudTrailComponent.h"
#include "B1MudTrailSubsystem.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1VehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"

UB1MudTrailComponent::UB1MudTrailComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UB1MudTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UB1MudTrailSubsystem* Trails = GetWorld()->GetSubsystem<UB1MudTrailSubsystem>();
	const UB1VehicleMovementComponent* Movement = GetOwner()->FindComponentByClass<UB1VehicleMovementComponent>();
	if (!Trails || !Movement)
	{
		return;
	}

	LastStamp.SetNum(Movement->GetNumWheels());
	for (int32 WheelIndex = 0; WheelIndex < Movement->GetNumWheels(); ++WheelIndex)
	{
		const FWheelStatus& Status = Movement->GetWheelState(WheelIndex);
		if (!Status.bInContact || !Cast<UB1PhysicalMaterial_Mud>(Status.PhysMaterial.Get()))
		{
			LastStamp[WheelIndex].Reset();
			continue;
		}

		const FVector Contact = Status.HitLocation;
		TOptional<FVector>& Last = LastStamp[WheelIndex];
		if (Last.IsSet() && FVector::Dist2D(Contact, Last.GetValue()) < UB1MudTrailSubsystem::StampSpacing)
		{
			continue;
		}

		// A barely sunk wheel would only fill older ruts (the stamp blends toward its depth), so it leaves no mark.
		const float Depth = Movement->GetWheelSinkDepth(WheelIndex);
		if (Depth >= MinRutDepth)
		{
			const UChaosVehicleWheel* Wheel = Movement->Wheels.IsValidIndex(WheelIndex) ? Movement->Wheels[WheelIndex].Get() : nullptr;
			const float Width = (Wheel ? Wheel->WheelWidth : 30.0f) * WidthScale + Depth * WallWidthPerDepth;
			Trails->AddSegment(Last.Get(Contact), Contact, Width, Depth);
		}
		Last = Contact;
	}
}
