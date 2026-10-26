#pragma once

#include "CoreMinimal.h"
#include "MudDriverOffroadCar.h"
#include "AbilitySystemInterface.h"
#include "B1Jeep.generated.h"

class UB1VehicleMovementComponent;
class UAbilitySystemComponent;
class UB1VehicleTuningData;
struct FOnAttributeChangeData;

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
	virtual void Tick(float DeltaSeconds) override;

	UB1VehicleTuningData* GetTuningData() const { return TuningData; }

	/** L/h this frame; 0 while nobody drives */
	float GetFuelRate() const { return FuelRate; }

	/** Throttle × average sink depth over MudFullStrainDepth, [0, 1]; scales MudWorkRate */
	float GetMudStrain() const { return MudStrain; }

	/** Over the last few seconds; negative until the jeep has moved at least a metre in that window */
	float GetFuelPer100m() const;

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Fuel|Config")
	TObjectPtr<UB1VehicleTuningData> TuningData;

private:

	/** ADR-010: accumulates IdleRate + WorkRate × power and applies it as one drain GE per DrainInterval */
	void ConsumeFuel(float DeltaSeconds);

	/** At 0 fuel: Vehicle.State.OutOfFuel and the movement's throttle cut-off */
	void UpdateOutOfFuel(float Fuel);
	void OnFuelChanged(const FOnAttributeChangeData& Data);
	FDelegateHandle FuelChangedHandle;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;

	float FuelRate = 0.0f;
	float MudStrain = 0.0f;
	float PendingDrain = 0.0f;
	float DrainTimer = 0.0f;
	FVector LastDrainLocation = FVector::ZeroVector;

	/** One entry per drain: litres and metres */
	TArray<FVector2f> FuelWindow;
};
