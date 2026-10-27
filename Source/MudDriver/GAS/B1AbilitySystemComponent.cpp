#include "B1AbilitySystemComponent.h"
#include "B1GASSettings.h"
#include "B1GameplayTags.h"
#include "B1VehicleAttributeSet.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"

bool UB1AbilitySystemComponent::ApplySetByCallerEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, FGameplayTag DataTag, float Magnitude)
{
	if (!EffectClass)
	{
		return false;
	}

	const FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, 1.f, MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(DataTag, Magnitude);
	ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
}

bool UB1AbilitySystemComponent::ApplyVehicleDamage(float Amount)
{
	return ApplySetByCallerEffectToSelf(GetDefault<UB1GASSettings>()->DamageEffect.LoadSynchronous(), B1GameplayTags::Data_Damage, Amount);
}

bool UB1AbilitySystemComponent::SetVehicleCondition(float Value)
{
	// Negative damage repairs; the attribute set clamps to [0, MaxCondition].
	return ApplyVehicleDamage(GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute()) - Value);
}

namespace
{
	FPredictionKey GetOriginalPredictionKey(const FGameplayAbilitySpec& Spec)
	{
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		return Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
	}
}

void UB1AbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// Collected first: activation can change ActivatableAbilities.
	TArray<FGameplayAbilitySpecHandle> ToActivate;
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		if (Spec.IsActive())
		{
			AbilitySpecInputPressed(Spec);
			// For WaitInputPress tasks.
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, GetOriginalPredictionKey(Spec));
		}
		else
		{
			Spec.InputPressed = true;
			// Judged before any activation: exiting the jeep must not let the same press interact on foot.
			if (Spec.Ability->CanActivateAbility(Spec.Handle, AbilityActorInfo.Get()))
			{
				ToActivate.Add(Spec.Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : ToActivate)
	{
		TryActivateAbility(Handle);
	}
}

void UB1AbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, GetOriginalPredictionKey(Spec));
		}
		else
		{
			Spec.InputPressed = false;
		}
	}
}

bool UB1AbilitySystemComponent::AddVehicleFuel(float Litres)
{
	const UB1GASSettings* Settings = GetDefault<UB1GASSettings>();
	const TSoftClassPtr<UGameplayEffect>& Effect = Litres >= 0.f ? Settings->FuelAddEffect : Settings->FuelDrainEffect;
	return ApplySetByCallerEffectToSelf(Effect.LoadSynchronous(), B1GameplayTags::Data_Fuel, Litres);
}
