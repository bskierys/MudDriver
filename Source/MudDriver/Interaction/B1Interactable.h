#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "B1Interactable.generated.h"

class AB1Character;

/** One thing the interact key can do; GA_B1Interact sends TriggerEventTag to the player's ASC (ADR-017) */
USTRUCT(BlueprintType)
struct FB1InteractionOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "B1|Interaction")
	FText Text;

	/** Gameplay event that triggers the target ability */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "B1|Interaction", meta = (Categories = "Event"))
	FGameplayTag TriggerEventTag;

	/** Higher wins; distance to InteractionPoint breaks ties */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "B1|Interaction")
	int32 Priority = 0;

	/** World space; must be within the scan range of the character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "B1|Interaction")
	FVector InteractionPoint = FVector::ZeroVector;

	/** Sent as the event's target; filled by the scan */
	UPROPERTY(BlueprintReadOnly, Category = "B1|Interaction")
	TWeakObjectPtr<AActor> TargetActor;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UB1Interactable : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by an actor or one of its components; Lyra's IInteractableTarget without per-option abilities */
class MUDDRIVER_API IB1Interactable
{
	GENERATED_BODY()

public:

	virtual void GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const = 0;
};
