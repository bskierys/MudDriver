#include "B1WinchAnchorComponent.h"
#include "UObject/UObjectIterator.h"

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
