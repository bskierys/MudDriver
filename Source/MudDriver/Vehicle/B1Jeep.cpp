#include "B1Jeep.h"
#include "B1VehicleMovementComponent.h"

AB1Jeep::AB1Jeep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UB1VehicleMovementComponent>(AWheeledVehiclePawn::VehicleMovementComponentName))
{
}

UB1VehicleMovementComponent* AB1Jeep::GetB1VehicleMovement() const
{
	return CastChecked<UB1VehicleMovementComponent>(GetVehicleMovementComponent());
}
