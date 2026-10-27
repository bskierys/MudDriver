#include "B1AbilitySet.h"
#include "B1Log.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

void FB1AbilitySet_GrantedHandles::TakeFromAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		ASC->ClearAbility(Handle);
	}
	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}
	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}

void UB1AbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, FB1AbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (int32 Index = 0; Index < GrantedGameplayAbilities.Num(); ++Index)
	{
		const FB1AbilitySet_GameplayAbility& ToGrant = GrantedGameplayAbilities[Index];
		if (!ToGrant.Ability)
		{
			UE_LOG(LogB1Ability, Error, TEXT("%s: GrantedGameplayAbilities[%d] has no ability"), *GetNameSafe(this), Index);
			continue;
		}

		FGameplayAbilitySpec Spec(ToGrant.Ability, ToGrant.AbilityLevel);
		Spec.SourceObject = SourceObject;
		Spec.GetDynamicSpecSourceTags().AddTag(ToGrant.InputTag);

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AbilitySpecHandles.Add(Handle);
		}
	}

	for (int32 Index = 0; Index < GrantedGameplayEffects.Num(); ++Index)
	{
		const FB1AbilitySet_GameplayEffect& ToGrant = GrantedGameplayEffects[Index];
		if (!ToGrant.GameplayEffect)
		{
			UE_LOG(LogB1Ability, Error, TEXT("%s: GrantedGameplayEffects[%d] has no effect"), *GetNameSafe(this), Index);
			continue;
		}

		const UGameplayEffect* Effect = ToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(Effect, ToGrant.EffectLevel, ASC->MakeEffectContext());
		if (OutGrantedHandles)
		{
			OutGrantedHandles->GameplayEffectHandles.Add(Handle);
		}
	}
}
