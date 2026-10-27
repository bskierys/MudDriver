#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "B1MudSprayComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UB1MudVehicleComponent;

/** Mud thrown from spinning wheels: one Niagara component per wheel, fed slip, direction and mud colour each tick */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1MudSprayComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UB1MudSprayComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Unset uses UB1MudSettings::SpraySystem */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray")
	TObjectPtr<UNiagaraSystem> SpraySystem;

	/** Wheel surface speed over ground speed below this doesn't spray */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "m/s", ClampMin = "0"))
	float MinSlipSpeed = 0.5f;

	/** Particles/s per m/s of slip above MinSlipSpeed */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (ClampMin = "0"))
	float SpawnRatePerSlip = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (ClampMin = "0"))
	float MaxSpawnRate = 400.0f;

	/** Launch speed added per unit of slip speed, on top of MinSpraySpeed */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (ClampMin = "0"))
	float SpraySpeedPerSlip = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "cm/s", ClampMin = "0"))
	float MinSpraySpeed = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "cm/s", ClampMin = "0"))
	float MaxSpraySpeed = 900.0f;

	/** Elevation of the spray above the horizontal, opposite to the wheel spin */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "deg", ClampMin = "0", ClampMax = "90"))
	float SprayElevation = 35.0f;

	/** Per wheel, particles/s this frame */
	float GetWheelSpawnRate(int32 WheelIndex) const { return SpawnRates.IsValidIndex(WheelIndex) ? SpawnRates[WheelIndex] : 0.0f; }

protected:

	virtual void BeginPlay() override;

private:

	void CreateWheelEmitters();

	UPROPERTY(Transient)
	TObjectPtr<UB1MudVehicleComponent> MudVehicle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> WheelEmitters;

	TArray<float> SpawnRates;
};
