#pragma once

#include "CoreMinimal.h"

class UChaosWheeledVehicleMovementComponent;
class UB1VehicleMovementComponent;
class AActor;

namespace B1VehicleTelemetry
{
	/** Seconds between telemetry lines (2 Hz) */
	static constexpr float Interval = 0.5f;

	bool IsEnabled();
	bool IsMudEnabled();
	bool IsStatusEnabled();

	/** One LogB1Vehicle line with speed and per-wheel contact state */
	void LogWheels(const UChaosWheeledVehicleMovementComponent& Movement);

	/** One LogB1Physics line with per-wheel mud state and belly drag; skipped while nothing touches mud */
	void LogMud(const UB1VehicleMovementComponent& Movement);

	/** One LogB1Vehicle line with condition, fuel and ASC tags; skipped for vehicles without an ASC */
	void LogStatus(const AActor& Vehicle);
}
