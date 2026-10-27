#include "B1InteractionScanComponent.h"
#include "B1Character.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

UB1InteractionScanComponent::UB1InteractionScanComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UB1InteractionScanComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SetComponentTickInterval(ScanInterval);
	Scan();
}

void UB1InteractionScanComponent::Scan()
{
	Options.Reset();

	const AB1Character* Character = Cast<AB1Character>(GetOwner());
	if (!Character || !Character->IsPlayerControlled() || Character->IsSeated())
	{
		return;
	}

	const FVector Origin = Character->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(B1InteractionScan), false, Character);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams::AllObjects,
		FCollisionShape::MakeSphere(Range), Params);

	TSet<const UObject*> Visited;
	auto Gather = [&](const UObject* Object, AActor* Actor)
	{
		const IB1Interactable* Interactable = Cast<IB1Interactable>(Object);
		bool bAlreadyVisited = false;
		Visited.Add(Object, &bAlreadyVisited);
		if (!Interactable || bAlreadyVisited)
		{
			return;
		}

		TArray<FB1InteractionOption> Gathered;
		Interactable->GatherInteractionOptions(*Character, Gathered);
		for (FB1InteractionOption& Option : Gathered)
		{
			if (Option.TriggerEventTag.IsValid() && FVector::Dist(Origin, Option.InteractionPoint) <= Range)
			{
				Option.TargetActor = Actor;
				Options.Add(MoveTemp(Option));
			}
		}
	};

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor)
		{
			continue;
		}
		Gather(Actor, Actor);
		for (const UActorComponent* Component : Actor->GetComponentsByInterface(UB1Interactable::StaticClass()))
		{
			Gather(Component, Actor);
		}
	}

	Options.Sort([&Origin](const FB1InteractionOption& A, const FB1InteractionOption& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}
		return FVector::DistSquared(Origin, A.InteractionPoint) < FVector::DistSquared(Origin, B.InteractionPoint);
	});
}

const FB1InteractionOption* UB1InteractionScanComponent::GetBestOption() const
{
	return Options.Num() > 0 ? &Options[0] : nullptr;
}
