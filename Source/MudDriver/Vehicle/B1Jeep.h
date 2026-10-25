#pragma once

#include "CoreMinimal.h"
#include "MudDriverOffroadCar.h"
#include "B1Jeep.generated.h"

class UB1VehicleMovementComponent;

/** The player's jeep: the template offroad car running on UB1VehicleMovementComponent */
UCLASS(abstract)
class AB1Jeep : public AMudDriverOffroadCar
{
	GENERATED_BODY()

public:

	AB1Jeep(const FObjectInitializer& ObjectInitializer);

	UB1VehicleMovementComponent* GetB1VehicleMovement() const;
};
