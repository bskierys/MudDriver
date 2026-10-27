#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "B1InputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FB1InputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "B1|Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "B1|Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

// Input actions that activate abilities by input tag (ADR-014). Continuous controls stay native on the pawns.
UCLASS(BlueprintType, Const)
class MUDDRIVER_API UB1InputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "B1|Input", meta = (TitleProperty = InputAction))
	TArray<FB1InputAction> AbilityInputActions;
};
