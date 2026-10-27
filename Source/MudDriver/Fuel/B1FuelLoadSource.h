#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "B1FuelLoadSource.generated.h"

/** One extra engine load; UB1FuelConsumptionData::LoadRates maps Name to L/h at Load 1 */
struct FB1FuelLoad
{
	FName Name;

	/** [0, 1] */
	float Load = 0.0f;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UB1FuelLoadSource : public UInterface
{
	GENERATED_BODY()
};

/** A component on the vehicle that adds engine load beyond engine power, e.g. mud strain */
class MUDDRIVER_API IB1FuelLoadSource
{
	GENERATED_BODY()

public:

	virtual void AddFuelLoads(TArray<FB1FuelLoad>& OutLoads) const = 0;
};
