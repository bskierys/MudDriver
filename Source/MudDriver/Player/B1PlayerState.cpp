#include "B1PlayerState.h"
#include "B1AbilitySystemComponent.h"
#include "B1VehicleAttributeSet.h"

AB1PlayerState::AB1PlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UB1AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Attribute sets created as default subobjects of the ASC owner are registered automatically.
	VehicleAttributeSet = CreateDefaultSubobject<UB1VehicleAttributeSet>(TEXT("VehicleAttributeSet"));
}

UAbilitySystemComponent* AB1PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
