#include "B1GameplayAbility_EnterVehicle.h"
#include "B1GameplayTags.h"
#include "B1Jeep.h"
#include "B1PlayerController.h"

UB1GameplayAbility_EnterVehicle::UB1GameplayAbility_EnterVehicle()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationRequiredTags.AddTag(B1GameplayTags::State_Avatar_OnFoot);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = B1GameplayTags::Event_Interact_EnterVehicle;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UB1GameplayAbility_EnterVehicle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AB1PlayerController* PC = ActorInfo ? Cast<AB1PlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
	AB1Jeep* Jeep = TriggerEventData ? const_cast<AB1Jeep*>(Cast<AB1Jeep>(TriggerEventData->Target.Get())) : nullptr;
	if (!PC || !Jeep || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bEntered = PC->EnterVehicle(Jeep);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bEntered);
}
