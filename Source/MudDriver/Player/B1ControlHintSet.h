#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "B1ControlHintSet.generated.h"

class UInputAction;

/** One line of the controls hints: keys come from the applied mapping contexts, so a rebind updates it */
USTRUCT(BlueprintType)
struct FB1ControlHint
{
	GENERATED_BODY()

	/** Keys of all of these, e.g. mouse look and stick look on one line */
	UPROPERTY(EditAnywhere, Category = "Hint")
	TArray<TObjectPtr<UInputAction>> Actions;

	UPROPERTY(EditAnywhere, Category = "Hint")
	FText Label;

	/** All of these on the player's ASC */
	UPROPERTY(EditAnywhere, Category = "Hint")
	FGameplayTagContainer RequiredTags;

	/** None of these on the player's ASC */
	UPROPERTY(EditAnywhere, Category = "Hint")
	FGameplayTagContainer BlockedTags;

	/** Keys shown per device, e.g. 2 for "A D" steering */
	UPROPERTY(EditAnywhere, Category = "Hint", meta = (ClampMin = "1"))
	int32 KeysPerDevice = 1;
};

/** Controls hints in the lower-left corner, filtered by the player's tags (S4-19) */
UCLASS(BlueprintType)
class MUDDRIVER_API UB1ControlHintSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Hints")
	TArray<FB1ControlHint> Hints;
};
