#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "B1GameplayAbility_Interact.generated.h"

// Sends the scan's best option as a gameplay event; the option's own ability triggers from it (ADR-017).
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_Interact : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_Interact();

	/** Fails with nothing in range, so a press there activates nothing */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
