#pragma once

#include "CoreMinimal.h"

class UChaosWheeledVehicleMovementComponent;

namespace B1VehicleTelemetry
{
	/** Seconds between telemetry lines (2 Hz) */
	static constexpr float Interval = 0.5f;

	bool IsEnabled();

	/** One LogB1Vehicle line with speed and per-wheel contact state */
	void LogWheels(const UChaosWheeledVehicleMovementComponent& Movement);
}
