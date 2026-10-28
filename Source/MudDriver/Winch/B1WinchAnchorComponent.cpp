#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "B1Character.h"
#include "B1GameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UObjectIterator.h"

void UB1WinchAnchorComponent::GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const
{
	FB1InteractionOption Option;
	Option.Priority = 10;
	Option.InteractionPoint = GetClosestAreaPoint(Character.GetActorLocation());

	if (const UB1WinchComponent* Carried = UB1WinchComponent::FindCarriedBy(&Character))
	{
		if (!Carried->IsInReach(this))
		{
			return;
		}
		Option.Text = NSLOCTEXT("B1Winch", "AttachHook", "Attach");
		Option.TriggerEventTag = B1GameplayTags::Event_Interact_AttachHook;
	}
	else if (UB1WinchComponent::FindAttachedTo(this))
	{
		Option.Text = NSLOCTEXT("B1Winch", "DetachHook", "Detach");
		Option.TriggerEventTag = B1GameplayTags::Event_Interact_DetachHook;
	}
	else
	{
		return;
	}
	Options.Add(MoveTemp(Option));
}

UPrimitiveComponent* UB1WinchAnchorComponent::GetSimulatingBody() const
{
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetAttachParent());
	return Body && Body->IsSimulatingPhysics() && Body->GetBodyInstance() ? Body : nullptr;
}

FVector UB1WinchAnchorComponent::ToWorld(const FVector& Local) const
{
	return GetComponentLocation() + GetComponentQuat().RotateVector(Local);
}

FVector UB1WinchAnchorComponent::GetAreaPoint(float Alpha) const
{
	return ToWorld(FMath::Lerp(AreaStart, AreaEnd, FMath::Clamp(Alpha, 0.0f, 1.0f)));
}

float UB1WinchAnchorComponent::GetAreaAlphaClosestTo(const FVector& WorldPoint) const
{
	const FVector Start = ToWorld(AreaStart);
	const FVector Segment = ToWorld(AreaEnd) - Start;
	const double LengthSq = Segment.SizeSquared();
	return LengthSq > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(static_cast<float>(((WorldPoint - Start) | Segment) / LengthSq), 0.0f, 1.0f) : 0.0f;
}

float UB1WinchAnchorComponent::GetAreaAlphaClosestToRay(const FVector& Origin, const FVector& Direction, float Length) const
{
	FVector OnArea;
	FVector OnRay;
	FMath::SegmentDistToSegmentSafe(ToWorld(AreaStart), ToWorld(AreaEnd), Origin, Origin + Direction.GetSafeNormal() * Length, OnArea, OnRay);
	return GetAreaAlphaClosestTo(OnArea);
}

FVector UB1WinchAnchorComponent::GetAreaAxis() const
{
	const FVector Axis = GetComponentQuat().RotateVector(AreaEnd - AreaStart);
	return Axis.IsNearlyZero() ? GetUpVector() : Axis.GetSafeNormal();
}

FVector UB1WinchAnchorComponent::GetSurfacePoint(float Alpha, const FVector& Toward, float Clearance) const
{
	const FVector Center = GetAreaPoint(Alpha);
	const FVector Radial = FVector::VectorPlaneProject(Toward - Center, GetAreaAxis()).GetSafeNormal();
	return Center + Radial * (TrunkRadius + Clearance);
}

void UB1WinchAnchorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	while (UB1WinchComponent* Winch = UB1WinchComponent::FindAttachedTo(this))
	{
		Winch->Detach();
	}

	Super::EndPlay(EndPlayReason);
}

UB1WinchAnchorComponent* UB1WinchAnchorComponent::FindNearest(const UWorld* World, const FVector& Location, float MaxDistance)
{
	UB1WinchAnchorComponent* Nearest = nullptr;
	float NearestDistSq = FMath::Square(MaxDistance);

	for (UB1WinchAnchorComponent* Anchor : TObjectRange<UB1WinchAnchorComponent>())
	{
		if (Anchor->GetWorld() != World || !Anchor->IsRegistered())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Location, Anchor->GetClosestAreaPoint(Location));
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Anchor;
		}
	}
	return Nearest;
}
