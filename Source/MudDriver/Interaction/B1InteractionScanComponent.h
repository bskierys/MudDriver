#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "B1Interactable.h"
#include "B1InteractionScanComponent.generated.h"

/** Finds the interaction options around an on-foot character and keeps the best one (ADR-017) */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1InteractionScanComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UB1InteractionScanComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Runs the overlap now; the tick does the same at ScanInterval */
	void Scan();

	/** Null when nothing is in range */
	const FB1InteractionOption* GetBestOption() const;
	const TArray<FB1InteractionOption>& GetOptions() const { return Options; }

	UPROPERTY(EditAnywhere, Category = "Interaction|Config", meta = (Units = "cm", ClampMin = "0"))
	float Range = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Interaction|Config", meta = (Units = "s", ClampMin = "0"))
	float ScanInterval = 0.1f;

private:

	/** In range, best first */
	TArray<FB1InteractionOption> Options;
};
