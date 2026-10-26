#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "B1VehicleAttributeSet.generated.h"

// Single-player prototype: attributes are not replicated.
UCLASS()
class MUDDRIVER_API UB1VehicleAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UB1VehicleAttributeSet();

	ATTRIBUTE_ACCESSORS_BASIC(UB1VehicleAttributeSet, Condition)
	ATTRIBUTE_ACCESSORS_BASIC(UB1VehicleAttributeSet, MaxCondition)
	ATTRIBUTE_ACCESSORS_BASIC(UB1VehicleAttributeSet, Fuel)
	ATTRIBUTE_ACCESSORS_BASIC(UB1VehicleAttributeSet, MaxFuel)
	ATTRIBUTE_ACCESSORS_BASIC(UB1VehicleAttributeSet, Damage)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Condition", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Condition;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Condition", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxCondition;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Fuel", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Fuel;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Fuel", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxFuel;

	// Meta attribute: GEs write damage here, PostGameplayEffectExecute moves it to Condition (Lyra pattern).
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Condition", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Damage;
};
