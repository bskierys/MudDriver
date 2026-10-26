#include "B1VehicleAttributeSet.h"
#include "GameplayEffectExtension.h"

UB1VehicleAttributeSet::UB1VehicleAttributeSet()
{
	InitCondition(100.f);
	InitMaxCondition(100.f);
	InitFuel(60.f);
	InitMaxFuel(60.f);
	InitDamage(0.f);
}

void UB1VehicleAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetConditionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxCondition());
	}
	else if (Attribute == GetFuelAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxFuel());
	}
	else if (Attribute == GetMaxConditionAttribute() || Attribute == GetMaxFuelAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UB1VehicleAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f);
		SetCondition(FMath::Clamp(GetCondition() - LocalDamage, 0.f, GetMaxCondition()));
	}
	else if (Data.EvaluatedData.Attribute == GetConditionAttribute())
	{
		SetCondition(FMath::Clamp(GetCondition(), 0.f, GetMaxCondition()));
	}
	else if (Data.EvaluatedData.Attribute == GetFuelAttribute())
	{
		SetFuel(FMath::Clamp(GetFuel(), 0.f, GetMaxFuel()));
	}
}
