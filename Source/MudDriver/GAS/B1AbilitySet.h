#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "B1AbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FB1AbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "B1|Abilities")
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, Category = "B1|Abilities")
	int32 AbilityLevel = 1;

	/** Added to the spec's dynamic tags; UB1AbilitySystemComponent activates by it */
	UPROPERTY(EditDefaultsOnly, Category = "B1|Abilities", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FB1AbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "B1|Abilities")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditDefaultsOnly, Category = "B1|Abilities")
	float EffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct FB1AbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void TakeFromAbilitySystem(UAbilitySystemComponent* ASC);

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

// Abilities with input tags plus effects, granted once to the PlayerState ASC (ADR-014).
UCLASS(BlueprintType, Const)
class MUDDRIVER_API UB1AbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Authority only */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FB1AbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "B1|Abilities", meta = (TitleProperty = Ability))
	TArray<FB1AbilitySet_GameplayAbility> GrantedGameplayAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "B1|Abilities", meta = (TitleProperty = GameplayEffect))
	TArray<FB1AbilitySet_GameplayEffect> GrantedGameplayEffects;
};
