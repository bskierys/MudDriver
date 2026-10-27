#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "B1Interactable.h"
#include "B1TestInteractable.generated.h"

class USphereComponent;

/** Offers one "Ping" option that triggers GA_B1TestPing; for interaction rigs */
UCLASS()
class MUDDRIVERTESTS_API AB1TestInteractable : public AActor, public IB1Interactable
{
	GENERATED_BODY()

public:

	AB1TestInteractable();

	virtual void GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const override;

protected:

	/** Query-only, so the scan's overlap finds it and nothing collides with it */
	UPROPERTY(VisibleAnywhere, Category = "B1Test")
	TObjectPtr<USphereComponent> Sphere;
};
