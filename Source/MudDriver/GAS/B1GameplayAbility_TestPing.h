#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "B1GameplayAbility_TestPing.generated.h"

// Logs and ends; proves the input tag → ability path (S4-02).
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_TestPing : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_TestPing();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	int32 ActivationCount = 0;
};
