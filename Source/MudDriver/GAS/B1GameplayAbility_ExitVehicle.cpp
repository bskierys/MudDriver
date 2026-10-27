#include "B1GameplayAbility_ExitVehicle.h"
#include "B1GameplayTags.h"
#include "B1PlayerController.h"

UB1GameplayAbility_ExitVehicle::UB1GameplayAbility_ExitVehicle()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationRequiredTags.AddTag(B1GameplayTags::State_Avatar_InVehicle);
}

void UB1GameplayAbility_ExitVehicle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AB1PlayerController* PC = ActorInfo ? Cast<AB1PlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
	if (!PC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bExited = PC->ExitVehicle();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bExited);
}
