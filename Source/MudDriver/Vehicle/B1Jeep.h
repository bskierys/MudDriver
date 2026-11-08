#pragma once

#include "CoreMinimal.h"
#include "MudDriverOffroadCar.h"
#include "AbilitySystemInterface.h"
#include "B1Interactable.h"
#include "B1Jeep.generated.h"

class UB1VehicleMovementComponent;
class UAbilitySystemComponent;
class UB1FuelConsumptionComponent;
class UB1MudDirtComponent;
class UB1MudSprayComponent;
class UB1MudTrailComponent;
class UB1MudVehicleComponent;
class UB1WinchComponent;
class UInputAction;
struct FInputActionValue;

/** The player's jeep: the template offroad car running on UB1VehicleMovementComponent */
UCLASS(abstract)
class MUDDRIVER_API AB1Jeep : public AMudDriverOffroadCar, public IAbilitySystemInterface, public IB1Interactable
{
	GENERATED_BODY()

public:

	AB1Jeep(const FObjectInitializer& ObjectInitializer);

	UB1VehicleMovementComponent* GetB1VehicleMovement() const;

	// The PlayerState ASC of the last driver; kept after unpossess so attributes stay reachable (ADR-004).
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UB1MudVehicleComponent* GetMudVehicle() const { return MudVehicle; }
	UB1MudDirtComponent* GetMudDirt() const { return MudDirt; }
	UB1FuelConsumptionComponent* GetFuelConsumption() const { return FuelConsumption; }
	UB1WinchComponent* GetWinch() const { return Winch; }
	USceneComponent* GetDriverSeat() const { return DriverSeat; }
	USceneComponent* GetDriverDoor() const { return DriverDoor; }

	/** While nobody drives: "Enter" at the driver door, "Take hook" / "Return hook" at the winch (ADR-017) */
	virtual void GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const override;

	/** Parking brake on every wheel and engine off (Vehicle.State.EngineOff); possessing the jeep releases it */
	void SetParked(bool bParked);
	bool IsParked() const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mud|Components")
	TObjectPtr<UB1MudDirtComponent> MudDirt;

	/** Sits at the winch point on the front bumper */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Winch|Components")
	TObjectPtr<UB1WinchComponent> Winch;

	/** The on-foot character is attached here, hidden, while driving */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seat")
	TObjectPtr<USceneComponent> DriverSeat;

	/** Outside the driver door: the "Enter" interaction point and the first exit spot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seat")
	TObjectPtr<USceneComponent> DriverDoor;

	/** Axis1D, -1..1: positive reels in, negative reels out */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> WinchPullAction;

	virtual void DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos) override;

private:

	/** Out of fuel cuts the throttle; brake and steering keep working */
	void OnOutOfFuelChanged(bool bOutOfFuel);

	void WinchPull(const FInputActionValue& Value);

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
