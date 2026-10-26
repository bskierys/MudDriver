#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "B1AbilitySystemComponent.generated.h"

class UGameplayEffect;

UCLASS()
class MUDDRIVER_API UB1AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	bool ApplySetByCallerEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, FGameplayTag DataTag, float Magnitude);

	// Through the UB1GASSettings effects; false if the effect isn't set.
	bool ApplyVehicleDamage(float Amount);
	bool SetVehicleCondition(float Value);
	bool AddVehicleFuel(float Litres);
};
