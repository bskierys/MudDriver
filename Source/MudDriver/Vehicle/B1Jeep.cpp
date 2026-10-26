#include "B1Jeep.h"
#include "B1VehicleMovementComponent.h"
#include "B1GameplayTags.h"
#include "B1Log.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

AB1Jeep::AB1Jeep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UB1VehicleMovementComponent>(AWheeledVehiclePawn::VehicleMovementComponentName))
{
}

UB1VehicleMovementComponent* AB1Jeep::GetB1VehicleMovement() const
{
	return CastChecked<UB1VehicleMovementComponent>(GetVehicleMovementComponent());
}

UAbilitySystemComponent* AB1Jeep::GetAbilitySystemComponent() const
{
	return AbilitySystem.Get();
}

void AB1Jeep::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewController ? NewController->PlayerState : nullptr))
	{
		AbilitySystem = ASC;
	}

	UE_LOG(LogB1Vehicle, Log, TEXT("%s ASC link: %s"), *GetName(),
		*GetNameSafe(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(this)));
}

void AB1Jeep::UnPossessed()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::State_Avatar_InVehicle, 0);
	}

	Super::UnPossessed();
}
