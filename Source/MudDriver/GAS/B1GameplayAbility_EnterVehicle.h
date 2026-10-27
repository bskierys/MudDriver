#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "B1GameplayAbility_EnterVehicle.generated.h"

// Triggered by the jeep door's "Enter" option (event target = the jeep); AB1PlayerController::EnterVehicle does the swap.
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_EnterVehicle : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_EnterVehicle();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
