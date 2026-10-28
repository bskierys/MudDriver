#include "B1GameplayAbility_WinchHook.h"
#include "B1Character.h"
#include "B1GameplayTags.h"
#include "B1Jeep.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"

UB1GameplayAbility_WinchHook::UB1GameplayAbility_WinchHook()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationRequiredTags.AddTag(B1GameplayTags::State_Avatar_OnFoot);
}

void UB1GameplayAbility_WinchHook::SetTriggerEvent(const FGameplayTag& EventTag)
{
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = EventTag;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UB1GameplayAbility_WinchHook::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AB1Character* Character = ActorInfo ? Cast<AB1Character>(ActorInfo->AvatarActor.Get()) : nullptr;
	AActor* Target = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (!Character || !Target || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bDone = RunHookAction(*Character, *Target);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bDone);
}

UB1GameplayAbility_TakeHook::UB1GameplayAbility_TakeHook()
{
	SetTriggerEvent(B1GameplayTags::Event_Interact_TakeHook);
}

bool UB1GameplayAbility_TakeHook::RunHookAction(AB1Character& Character, AActor& Target) const
{
	const AB1Jeep* Jeep = Cast<AB1Jeep>(&Target);
	return Jeep && !UB1WinchComponent::FindCarriedBy(&Character) && Jeep->GetWinch()->TakeHook(Character.GetHookHand());
}

UB1GameplayAbility_ReturnHook::UB1GameplayAbility_ReturnHook()
{
	SetTriggerEvent(B1GameplayTags::Event_Interact_ReturnHook);
}

bool UB1GameplayAbility_ReturnHook::RunHookAction(AB1Character& Character, AActor& Target) const
{
	const AB1Jeep* Jeep = Cast<AB1Jeep>(&Target);
	UB1WinchComponent* Winch = Jeep ? Jeep->GetWinch() : nullptr;
	if (!Winch || Winch->GetHookCarrier() != &Character)
	{
		return false;
	}
	Winch->Detach();
	return true;
}

UB1GameplayAbility_AttachHook::UB1GameplayAbility_AttachHook()
{
	SetTriggerEvent(B1GameplayTags::Event_Interact_AttachHook);
}

bool UB1GameplayAbility_AttachHook::RunHookAction(AB1Character& Character, AActor& Target) const
{
	UB1WinchComponent* Winch = UB1WinchComponent::FindCarriedBy(&Character);
	UB1WinchAnchorComponent* Anchor = Winch ? Winch->GetAimAnchor() : nullptr;
	return Anchor && Anchor->GetOwner() == &Target && Winch->AttachTo(Anchor, Winch->GetAimAlpha());
}

UB1GameplayAbility_DetachHook::UB1GameplayAbility_DetachHook()
{
	SetTriggerEvent(B1GameplayTags::Event_Interact_DetachHook);
}

bool UB1GameplayAbility_DetachHook::RunHookAction(AB1Character& Character, AActor& Target) const
{
	UB1WinchComponent* Winch = UB1WinchComponent::FindAttachedTo(Target.FindComponentByClass<UB1WinchAnchorComponent>());
	return Winch && !UB1WinchComponent::FindCarriedBy(&Character) && Winch->DetachToCarrier(Character.GetHookHand());
}
