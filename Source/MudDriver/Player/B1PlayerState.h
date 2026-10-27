#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "B1AbilitySet.h"
#include "B1PlayerState.generated.h"

class UB1AbilitySystemComponent;
class UB1VehicleAttributeSet;

// Owns the single ASC (ADR-004); avatars are the jeep or, later, the on-foot character.
UCLASS()
class MUDDRIVER_API AB1PlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AB1PlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UB1AbilitySystemComponent* GetB1AbilitySystemComponent() const { return AbilitySystemComponent; }
	const UB1VehicleAttributeSet* GetVehicleAttributeSet() const { return VehicleAttributeSet; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	FB1AbilitySet_GrantedHandles GrantedAbilitySetHandles;

	UPROPERTY(VisibleAnywhere, Category = "B1|Abilities")
	TObjectPtr<UB1AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UB1VehicleAttributeSet> VehicleAttributeSet;
};
