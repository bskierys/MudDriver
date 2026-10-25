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

	/** Sink speed at full wheelspin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float SinkRate = 20.0f;

	/** Depth recovered per metre rolled with grip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mud|Config", meta = (ClampMin = "0"))
	float RiseRate = 5.0f;

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
