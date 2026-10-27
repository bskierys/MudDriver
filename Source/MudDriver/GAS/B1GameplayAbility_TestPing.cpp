#include "B1GameplayAbility_TestPing.h"
#include "B1Log.h"

UB1GameplayAbility_TestPing::UB1GameplayAbility_TestPing()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UB1GameplayAbility_TestPing::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	++ActivationCount;
	UE_LOG(LogB1Ability, Display, TEXT("TestPing activated #%d on %s (avatar %s)"), ActivationCount,
		*GetNameSafe(ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr), *GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
