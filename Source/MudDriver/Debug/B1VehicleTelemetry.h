#pragma once

#include "CoreMinimal.h"

class UChaosWheeledVehicleMovementComponent;
class UB1VehicleMovementComponent;

namespace B1VehicleTelemetry
{
	/** Seconds between telemetry lines (2 Hz) */
	static constexpr float Interval = 0.5f;

	bool IsEnabled();
	bool IsMudEnabled();

	/** One LogB1Vehicle line with speed and per-wheel contact state */
	void LogWheels(const UChaosWheeledVehicleMovementComponent& Movement);

	/** One LogB1Physics line with per-wheel mud state and belly drag; skipped while nothing touches mud */
	void LogMud(const UB1VehicleMovementComponent& Movement);
}
