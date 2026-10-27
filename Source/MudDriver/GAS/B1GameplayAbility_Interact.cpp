#include "B1GameplayAbility_Interact.h"
#include "B1Character.h"
#include "B1GameplayTags.h"
#include "B1InteractionScanComponent.h"
#include "B1Log.h"
#include "AbilitySystemComponent.h"

namespace
{
	UB1InteractionScanComponent* GetScan(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const AB1Character* Character = ActorInfo ? Cast<AB1Character>(ActorInfo->AvatarActor.Get()) : nullptr;
		return Character ? Character->GetInteractionScan() : nullptr;
	}
}

UB1GameplayAbility_Interact::UB1GameplayAbility_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationRequiredTags.AddTag(B1GameplayTags::State_Avatar_OnFoot);
}

bool UB1GameplayAbility_Interact::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// A fresh scan, so a press right after walking up doesn't wait for the next scan tick.
	UB1InteractionScanComponent* Scan = GetScan(ActorInfo);
	if (!Scan)
	{
		return false;
	}
	Scan->Scan();
	return Scan->GetBestOption() != nullptr;
}

void UB1GameplayAbility_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const UB1InteractionScanComponent* Scan = GetScan(ActorInfo);
	const FB1InteractionOption* Option = Scan ? Scan->GetBestOption() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!Option || !ASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = Option->TriggerEventTag;
	Payload.Instigator = ActorInfo->AvatarActor.Get();
	Payload.Target = Option->TargetActor.Get();
	UE_LOG(LogB1Ability, Display, TEXT("Interact '%s' on %s -> %s"), *Option->Text.ToString(), *GetNameSafe(Payload.Target), *Payload.EventTag.ToString());

	// Copied first: the triggered ability can run a scan, which rebuilds the option list.
	const FGameplayTag EventTag = Payload.EventTag;
	ASC->HandleGameplayEvent(EventTag, &Payload);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
