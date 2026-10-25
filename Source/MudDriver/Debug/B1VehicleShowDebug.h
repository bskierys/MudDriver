#pragma once

#include "CoreMinimal.h"

class UCanvas;
class UChaosWheeledVehicleMovementComponent;
class FDebugDisplayInfo;

namespace B1VehicleShowDebug
{
	/** `ShowDebug B1Vehicle` */
	extern const FName Category;

	/** Speed, gear, RPM and per-wheel state as text projected above the vehicle */
	void Draw(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const UChaosWheeledVehicleMovementComponent& Movement);
}
