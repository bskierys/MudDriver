#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "B1PhysicalMaterial_Mud.generated.h"

/**
 * Mud tuning per surface (ADR-001, ADR-002). Base Friction is the grip at zero depth;
 * the curves take normalized depth (SinkDepth / MaxSinkDepth, 0..1).
 */
UCLASS(BlueprintType)
class MUDDRIVER_API UB1PhysicalMaterial_Mud : public UPhysicalMaterial
{
	GENERATED_BODY()

public:

	UB1PhysicalMaterial_Mud();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (Units = "cm", ClampMin = "0"))
	float MaxSinkDepth = 35.0f;

	/** Depth a wheel settles to under its resting load, at any speed, as a fraction of MaxSinkDepth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float StaticSinkRatio = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float SettleRate = 25.0f;

	/** Extra sink speed at full wheelspin, on top of settling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float SinkRate = 30.0f;

	/** Wheel surface speed over ground speed that counts as full wheelspin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (Units = "m/s", ClampMin = "0.1"))
	float FullSinkSlipSpeed = 3.0f;

	/** Depth recovered per metre rolled with grip, down to the static depth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (ClampMin = "0"))
	float RiseRate = 2.0f;

	/** Multiplies the tire friction */
	UPROPERTY(EditAnywhere, Category = "Mud|Config")
	FRuntimeFloatCurve FrictionVsDepth;

	/** Rolling resistance as a fraction of the wheel load */
	UPROPERTY(EditAnywhere, Category = "Mud|Config")
	FRuntimeFloatCurve RollingResistanceVsDepth;

	/** Drag per metre of sink past ground clearance, as a fraction of vehicle weight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (ClampMin = "0"))
	float BellyDragCoefficient = 2.0f;
};
