#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Curves/RichCurve.h"
#include "B1VehicleSimInput.generated.h"

class UPhysicalMaterial;

UENUM()
enum class EB1TestForce : uint8
{
	Off,
	Push,
	Spring
};

/** Stand-ins for the river current (push) and the winch (spring) used to test physics stepping */
USTRUCT()
struct FB1TestForceSettings
{
	GENERATED_BODY()

	/** World +Y */
	UPROPERTY(EditAnywhere, Category = "TestForce|Config", meta = (Units = "N"))
	float PushForce = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "TestForce|Config", meta = (Units = "m"))
	float SpringRestLength = 5.0f;

	/** N/m */
	UPROPERTY(EditAnywhere, Category = "TestForce|Config")
	float SpringStiffness = 20000.0f;

	/** N*s/m. Near critical for the 1500 kg jeep; lower values make it bounce on the cable */
	UPROPERTY(EditAnywhere, Category = "TestForce|Config")
	float SpringDamping = 9000.0f;

	UPROPERTY(EditAnywhere, Category = "TestForce|Config", meta = (Units = "N"))
	float SpringMaxForce = 15000.0f;
};

/** Copy of UB1PhysicalMaterial_Mud for the physics thread, matched to the wheel hit by weak pointer */
struct FB1MudSimParams
{
	TWeakObjectPtr<UPhysicalMaterial> Material;
	float MaxSinkDepth = 0.0f;
	float StaticSinkDepth = 0.0f;
	float SettleRate = 0.0f;
	float SinkRate = 0.0f;
	float RiseRate = 0.0f;
	float FullSinkSlipSpeed = 1.0f;
	FRichCurve FrictionVsDepth;
	FRichCurve RollingResistanceVsDepth;
	float SpeedDragCoefficient = 0.0f;
	float BellyDragCoefficient = 0.0f;
};

/** Game thread → physics thread data, copied once per game frame */
struct FB1VehicleSimInput
{
	EB1TestForce TestForce = EB1TestForce::Off;
	FB1TestForceSettings TestForceSettings;
	FVector TestForceAnchor = FVector::ZeroVector;
	TArray<FB1MudSimParams> MudMaterials;

	/** cm, from the wheel contact to the underside of the chassis */
	float BellyClearance = 0.0f;
};

/** Physics thread → game thread, latest step */
struct FB1VehicleSimOutput
{
	/** Per wheel, cm */
	TArray<float> SinkDepth;

	/** Per wheel, mud friction curve value; 1 off mud */
	TArray<float> MudFrictionScale;

	/** Per wheel, N */
	TArray<float> RollingResistance;

	/** cm the average sink depth is past belly clearance */
	float BellyDepth = 0.0f;

	/** N */
	float BellyDrag = 0.0f;
};
