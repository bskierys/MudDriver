#pragma once

#include "CoreMinimal.h"

class UChaosWheeledVehicleMovementComponent;
class UB1MudVehicleComponent;
class UB1WinchComponent;
class AActor;

namespace B1VehicleTelemetry
{
	/** Seconds between telemetry lines (2 Hz) */
	static constexpr float Interval = 0.5f;

	bool IsEnabled();
	bool IsMudEnabled();
	bool IsStatusEnabled();
	bool IsWinchEnabled();

	/** One LogB1Vehicle line with speed and per-wheel contact state */
	void LogWheels(const UChaosWheeledVehicleMovementComponent& Movement);

	/** One LogB1Mud line with per-wheel mud state and belly drag; skipped while nothing touches mud */
	void LogMud(const UB1MudVehicleComponent& Mud);

	/** One LogB1Vehicle line with condition, fuel and ASC tags; skipped for vehicles without an ASC */
	void LogStatus(const AActor& Vehicle);

	/** One LogB1Physics line with anchor, cable length, rest, tension and pull; skipped while not attached */
	void LogWinch(const UB1WinchComponent& Winch);
}
