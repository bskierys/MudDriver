#include "B1VehicleTelemetry.h"
#include "B1Log.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1VehicleMovementComponent.h"
#include "B1VehicleAttributeSet.h"
#include "B1Jeep.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace B1VehicleTelemetry
{
	static TAutoConsoleVariable<bool> CVarVehicleTelemetry(
		TEXT("b1.Vehicle.Telemetry"),
		true,
		TEXT("Write a 2 Hz per-wheel telemetry line to LogB1Vehicle."));

	static TAutoConsoleVariable<bool> CVarMudTelemetry(
		TEXT("b1.Mud.Telemetry"),
		true,
		TEXT("Write a 2 Hz per-wheel mud line to LogB1Physics while the vehicle touches mud."));

	static TAutoConsoleVariable<bool> CVarStatusTelemetry(
		TEXT("b1.Vehicle.StatusTelemetry"),
		true,
		TEXT("Write a 2 Hz condition/fuel/tags line to LogB1Vehicle for vehicles with an ASC."));

	bool IsStatusEnabled()
	{
		return CVarStatusTelemetry.GetValueOnGameThread();
	}

	bool IsEnabled()
	{
		return CVarVehicleTelemetry.GetValueOnGameThread();
	}

	bool IsMudEnabled()
	{
		return CVarMudTelemetry.GetValueOnGameThread();
	}

	void LogWheels(const UChaosWheeledVehicleMovementComponent& Movement)
	{
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
		}

		UE_LOG(LogB1Vehicle, Log, TEXT("%s"), *Line);
	}

	void LogMud(const UB1VehicleMovementComponent& Movement)
	{
		bool bAnyMud = Movement.GetBellyDepth() > 0.0f;
		for (int32 WheelIndex = 0; WheelIndex < Movement.GetNumWheels() && !bAnyMud; ++WheelIndex)
		{
			bAnyMud = Movement.GetWheelSinkDepth(WheelIndex) > 0.0f || Cast<UB1PhysicalMaterial_Mud>(Movement.GetWheelState(WheelIndex).PhysMaterial.Get());
		}
		if (!bAnyMud)
		{
			return;
		}

		TStringBuilder<512> Line;
		Line.Appendf(TEXT("B1Mud %s v=%.1fkmh belly=%.1fcm/%.0fN"), *GetNameSafe(Movement.GetOwner()), Movement.GetForwardSpeed() * 0.036f,
			Movement.GetBellyDepth(), Movement.GetBellyDrag());

		for (int32 WheelIndex = 0; WheelIndex < Movement.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Movement.GetWheelState(WheelIndex);
			Line.Appendf(TEXT(" | w%d mat=%s sink=%.1f slip=%.2f mu=%.2f rr=%.0f"),
				WheelIndex,
				*GetNameSafe(Status.PhysMaterial.Get()),
				Movement.GetWheelSinkDepth(WheelIndex),
				Status.SlipMagnitude,
				Movement.GetWheelMudFrictionScale(WheelIndex),
				Movement.GetWheelRollingResistance(WheelIndex));
		}

		UE_LOG(LogB1Physics, Log, TEXT("%s"), *Line);
	}

	void LogStatus(const AActor& Vehicle)
	{
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Vehicle);
		if (!ASC)
		{
			return;
		}

		FGameplayTagContainer Tags;
		ASC->GetOwnedGameplayTags(Tags);

		const AB1Jeep* Jeep = Cast<AB1Jeep>(&Vehicle);
		UE_LOG(LogB1Vehicle, Log, TEXT("B1Status %s cond=%.1f/%.0f fuel=%.2f/%.0f lph=%.1f l100=%.2f pow=%.2f strain=%.2f tags=%s"),
			*Vehicle.GetName(),
			ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute()),
			ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetMaxConditionAttribute()),
			ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute()),
			ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetMaxFuelAttribute()),
			Jeep ? Jeep->GetFuelRate() : 0.0f,
			Jeep ? Jeep->GetFuelPer100m() : -1.0f,
			Jeep ? Jeep->GetB1VehicleMovement()->GetNormalizedEnginePower() : 0.0f,
			Jeep ? Jeep->GetMudStrain() : 0.0f,
			*Tags.ToStringSimple());
	}
}
