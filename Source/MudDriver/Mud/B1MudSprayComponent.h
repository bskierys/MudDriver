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

	/** Slip below this counts as rolling: the rate is scaled by RollingRateFactor */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "m/s", ClampMin = "0"))
	float MinSlipSpeed = 0.5f;

	/** Particles/s per wheel at B1MudThrow intensity 1; also the per-wheel cap */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (ClampMin = "0"))
	float MaxSpawnRate = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (ClampMin = "0", ClampMax = "1"))
	float RollingRateFactor = 0.4f;

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

	/** Cone half-angle around the throw direction (User.SpraySpread); droplets use 1.2x */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "deg", ClampMin = "0", ClampMax = "90"))
	float SpraySpread = 35.0f;

	/** Per-particle launch speed varies by +-this fraction (User.SpeedSpread), so some throw higher and farther */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (ClampMin = "0", ClampMax = "1"))
	float SpraySpeedSpread = 0.35f;

	/** Emitter distance behind the tread's contact edge (wheel radius + this). Too close: depth-buffer collision pins particles at birth */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "cm", ClampMin = "-50", ClampMax = "100"))
	float SpawnBackOffset = 20.0f;

	/** Emitter height above the visible mud surface */
	UPROPERTY(EditAnywhere, Category = "Mud|Spray", meta = (Units = "cm", ClampMin = "0", ClampMax = "100"))
	float SpawnLift = 20.0f;

	/** Per wheel, particles/s this frame */
	float GetWheelSpawnRate(int32 WheelIndex) const { return SpawnRates.IsValidIndex(WheelIndex) ? SpawnRates[WheelIndex] : 0.0f; }

	/** Per wheel, B1MudThrow intensity this frame (0 with the spray off) */
	float GetWheelIntensity(int32 WheelIndex) const { return Intensities.IsValidIndex(WheelIndex) ? Intensities[WheelIndex] : 0.0f; }

	/** Wheels with an emitter; 0 until the physics vehicle exists */
	int32 GetNumWheels() const { return SprayOrigins.Num(); }

	/** Per wheel, world emitter location this frame; zero when not spraying */
	FVector GetWheelSprayOrigin(int32 WheelIndex) const { return SprayOrigins.IsValidIndex(WheelIndex) ? SprayOrigins[WheelIndex] : FVector::ZeroVector; }

	/** Per wheel, world launch velocity (cm/s) this frame; zero when not spraying */
	FVector GetWheelSprayVelocity(int32 WheelIndex) const { return SprayVelocities.IsValidIndex(WheelIndex) ? SprayVelocities[WheelIndex] : FVector::ZeroVector; }

protected:

	virtual void BeginPlay() override;

private:

	void CreateWheelEmitters();

	UPROPERTY(Transient)
	TObjectPtr<UB1MudVehicleComponent> MudVehicle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> WheelEmitters;

	TArray<float> SpawnRates;
	TArray<float> Intensities;
	TArray<FVector> SprayOrigins;
	TArray<FVector> SprayVelocities;
};
