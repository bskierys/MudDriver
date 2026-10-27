#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "B1FuelConsumptionData.generated.h"

/** Fuel use rates for UB1FuelConsumptionComponent (ADR-010) */
UCLASS(BlueprintType)
class MUDDRIVER_API UB1FuelConsumptionData : public UDataAsset
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
	 * L/h at full load per IB1FuelLoadSource load name. "Mud" is the engine fighting mud (mud strain):
	 * engine power alone misses it, since Chaos torque ignores load and a slow jeep in mud runs low RPM.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel|Config")
	TMap<FName, float> LoadRates = { { TEXT("Mud"), 300.0f } };

	/** How often the accumulated consumption is applied as one drain effect */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel|Config", meta = (ClampMin = "0.02", Units = "s"))
	float DrainInterval = 0.25f;
};
