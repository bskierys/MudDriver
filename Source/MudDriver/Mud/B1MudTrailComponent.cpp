#include "B1MudTrailComponent.h"
#include "B1MudTrailSubsystem.h"
#include "B1MudVehicleComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"

namespace
{
	/** cm, when the host reports no wheel width */
	constexpr float FallbackWheelWidth = 30.0f;
}

UB1MudTrailComponent::UB1MudTrailComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UB1MudTrailComponent::BeginPlay()
{
	Super::BeginPlay();

	MudVehicle = GetOwner()->FindComponentByClass<UB1MudVehicleComponent>();
}

void UB1MudTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UB1MudTrailSubsystem* Trails = GetWorld()->GetSubsystem<UB1MudTrailSubsystem>();
	if (!Trails || !MudVehicle)
	{
		return;
	}

	LastStamp.SetNum(MudVehicle->GetNumWheels());
	for (int32 WheelIndex = 0; WheelIndex < MudVehicle->GetNumWheels(); ++WheelIndex)
	{
		if (!MudVehicle->GetWheelMud(WheelIndex))
		{
			LastStamp[WheelIndex].Reset();
			continue;
		}

		const FVector Contact = MudVehicle->GetWheelState(WheelIndex).HitLocation;
		TOptional<FVector>& Last = LastStamp[WheelIndex];
		if (Last.IsSet() && FVector::Dist2D(Contact, Last.GetValue()) < UB1MudTrailSubsystem::StampSpacing)
		{
			continue;
		}

		// A barely sunk wheel would only fill older ruts (the stamp blends toward its depth), so it leaves no mark.
		const float Depth = MudVehicle->GetWheelSinkDepth(WheelIndex);
		if (Depth >= MinRutDepth)
		{
			const float WheelWidth = MudVehicle->GetWheelWidth(WheelIndex);
			const float Width = (WheelWidth > 0.0f ? WheelWidth : FallbackWheelWidth) * WidthScale + Depth * WallWidthPerDepth;
			Trails->AddSegment(Last.Get(Contact), Contact, Width, Depth);
		}
		Last = Contact;
	}
}
