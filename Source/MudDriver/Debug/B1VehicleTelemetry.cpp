#include "B1VehicleTelemetry.h"
#include "B1Log.h"
#include "B1VehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace B1VehicleTelemetry
{
	static TAutoConsoleVariable<bool> CVarVehicleTelemetry(
		TEXT("b1.Vehicle.Telemetry"),
		true,
		TEXT("Write a 2 Hz per-wheel telemetry line to LogB1Vehicle."));

	bool IsEnabled()
	{
		return CVarVehicleTelemetry.GetValueOnGameThread();
	}

	void LogWheels(const UChaosWheeledVehicleMovementComponent& Movement)
	{
		const UB1VehicleMovementComponent* B1Movement = Cast<UB1VehicleMovementComponent>(&Movement);

		TStringBuilder<512> Line;
		Line.Appendf(TEXT("B1Wheels %s v=%.1fkmh"), *GetNameSafe(Movement.GetOwner()), Movement.GetForwardSpeed() * 0.036f);

		for (int32 WheelIndex = 0; WheelIndex < Movement.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Movement.GetWheelState(WheelIndex);
			Line.Appendf(TEXT(" | w%d c=%d mat=%s slip=%.2f%s skid=%.2f%s susp=%.2f drv=%.0f brk=%.0f"),
				WheelIndex,
				Status.bInContact ? 1 : 0,
				*GetNameSafe(Status.PhysMaterial.Get()),
				Status.SlipMagnitude, Status.bIsSlipping ? TEXT("!") : TEXT(""),
				Status.SkidMagnitude, Status.bIsSkidding ? TEXT("!") : TEXT(""),
				Status.NormalizedSuspensionLength,
				Status.DriveTorque,
				Status.BrakeTorque);

			if (B1Movement)
			{
				Line.Appendf(TEXT(" sink=%.1f"), B1Movement->GetWheelSinkDepth(WheelIndex));
			}
		}

		UE_LOG(LogB1Vehicle, Log, TEXT("%s"), *Line);
	}
}
