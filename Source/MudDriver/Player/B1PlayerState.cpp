#include "B1PlayerState.h"
#include "B1AbilitySystemComponent.h"
#include "B1GASSettings.h"
#include "B1VehicleAttributeSet.h"
#include "B1Log.h"

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

void AB1PlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	for (const TSoftObjectPtr<UB1AbilitySet>& SoftSet : GetDefault<UB1GASSettings>()->AbilitySets)
	{
		if (const UB1AbilitySet* Set = SoftSet.LoadSynchronous())
		{
			Set->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilitySetHandles, this);
		}
		else
		{
			UE_LOG(LogB1Ability, Error, TEXT("B1 GAS settings: ability set %s doesn't load"), *SoftSet.ToString());
		}
	}
}
