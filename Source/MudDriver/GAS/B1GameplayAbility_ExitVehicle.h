#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "B1GameplayAbility_ExitVehicle.generated.h"

// Instant: AB1PlayerController::ExitVehicle does the swap (S4-06).
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_ExitVehicle : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_ExitVehicle();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
