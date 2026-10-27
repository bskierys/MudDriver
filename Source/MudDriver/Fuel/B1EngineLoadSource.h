#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "B1EngineLoadSource.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UB1EngineLoadSource : public UInterface
{
	GENERATED_BODY()
};

/** The vehicle's engine as UB1FuelConsumptionComponent sees it; usually the movement component */
class MUDDRIVER_API IB1EngineLoadSource
{
	GENERATED_BODY()

public:

	/** Engine power over its peak, [0, 1] */
	virtual float GetNormalizedEnginePower() const = 0;
};
