#pragma once

#include "CoreMinimal.h"
#include "MudDriverOffroadCar.h"
#include "AbilitySystemInterface.h"
#include "B1Jeep.generated.h"

class UB1VehicleMovementComponent;
class UAbilitySystemComponent;
class UB1FuelConsumptionComponent;
class UB1MudSprayComponent;
class UB1MudTrailComponent;
class UB1MudVehicleComponent;

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

	UB1MudVehicleComponent* GetMudVehicle() const { return MudVehicle; }
	UB1FuelConsumptionComponent* GetFuelConsumption() const { return FuelConsumption; }

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fuel|Components")
	TObjectPtr<UB1FuelConsumptionComponent> FuelConsumption;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UB1MudVehicleComponent> MudVehicle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UB1MudSprayComponent> MudSpray;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UB1MudTrailComponent> MudTrail;

	virtual void DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos) override;

private:

	/** Out of fuel cuts the throttle; brake and steering keep working */
	void OnOutOfFuelChanged(bool bOutOfFuel);

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
