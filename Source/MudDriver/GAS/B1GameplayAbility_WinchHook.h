#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "B1GameplayAbility_WinchHook.generated.h"

class AB1Character;

// Instant on-foot hook action, triggered by an interaction option; the event target is the jeep or the anchor's actor.
UCLASS(abstract)
class MUDDRIVER_API UB1GameplayAbility_WinchHook : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_WinchHook();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	void SetTriggerEvent(const FGameplayTag& EventTag);

	virtual bool RunHookAction(AB1Character& Character, AActor& Target) const PURE_VIRTUAL(UB1GameplayAbility_WinchHook::RunHookAction, return false;);
};

// "Take hook" at the jeep front.
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_TakeHook : public UB1GameplayAbility_WinchHook
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_TakeHook();

protected:
	virtual bool RunHookAction(AB1Character& Character, AActor& Target) const override;
};

// "Return hook" at the jeep front.
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_ReturnHook : public UB1GameplayAbility_WinchHook
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_ReturnHook();

protected:
	virtual bool RunHookAction(AB1Character& Character, AActor& Target) const override;
};

// "Attach" at an anchor: the carried hook becomes a cable with rest length = distance (ADR-007).
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_AttachHook : public UB1GameplayAbility_WinchHook
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_AttachHook();

protected:
	virtual bool RunHookAction(AB1Character& Character, AActor& Target) const override;
};

// "Detach" at a hooked anchor: the hook goes back into the character's hand.
UCLASS()
class MUDDRIVER_API UB1GameplayAbility_DetachHook : public UB1GameplayAbility_WinchHook
{
	GENERATED_BODY()

public:
	UB1GameplayAbility_DetachHook();

protected:
	virtual bool RunHookAction(AB1Character& Character, AActor& Target) const override;
};
