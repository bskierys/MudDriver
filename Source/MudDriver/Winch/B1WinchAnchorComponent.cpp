#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "B1Character.h"
#include "B1GameplayTags.h"
#include "UObject/UObjectIterator.h"

void UB1WinchAnchorComponent::GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const
{
	FB1InteractionOption Option;
	Option.Priority = 10;
	Option.InteractionPoint = GetComponentLocation();

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

		const float DistSq = FVector::DistSquared(Location, Anchor->GetComponentLocation());
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Anchor;
		}
	}
	return Nearest;
}
