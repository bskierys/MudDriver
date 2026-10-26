#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "B1VehicleTuningData.generated.h"

/** Per-vehicle gameplay tuning that isn't Chaos setup (ADR-010) */
UCLASS(BlueprintType)
class MUDDRIVER_API UB1VehicleTuningData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** L/h with the engine running and no load */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel|Config", meta = (ClampMin = "0"))
	float IdleRate = 6.0f;

	/** L/h added on top of IdleRate at full engine power, scaled linearly by normalised power */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel|Config", meta = (ClampMin = "0"))
	float WorkRate = 240.0f;

	/**
	 * L/h added at full throttle with the wheels sunk to MudFullStrainDepth, for the engine fighting mud.
	 * Engine power alone misses it: Chaos torque ignores load and a slow jeep in mud runs low RPM.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel|Config", meta = (ClampMin = "0"))
	float MudWorkRate = 300.0f;

	/** Average wheel sink depth where mud strain reaches 1; strain scales linearly below it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel|Config", meta = (ClampMin = "1", Units = "cm"))
	float MudFullStrainDepth = 50.0f;

	/** How often the accumulated consumption is applied as one GE_B1FuelDrain */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel|Config", meta = (ClampMin = "0.02", Units = "s"))
	float DrainInterval = 0.25f;
};
