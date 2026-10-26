#pragma once

#include "CoreMinimal.h"
#include "MudDriverOffroadCar.h"
#include "AbilitySystemInterface.h"
#include "B1Jeep.generated.h"

class UB1VehicleMovementComponent;
class UAbilitySystemComponent;

/** The player's jeep: the template offroad car running on UB1VehicleMovementComponent */
UCLASS(abstract)
class AB1Jeep : public AMudDriverOffroadCar, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AB1Jeep(const FObjectInitializer& ObjectInitializer);

	UB1VehicleMovementComponent* GetB1VehicleMovement() const;

	// The PlayerState ASC of the last driver; kept after unpossess so attributes stay reachable (ADR-004).
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

private:

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
