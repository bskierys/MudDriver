#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "B1MudWheelModel.h"

class UPhysicalMaterial;

/** Copy of UB1PhysicalMaterial_Mud for the physics thread, matched to the wheel hit by weak pointer */
struct FB1MudSimParams : FB1MudWheelParams
{
	TWeakObjectPtr<UPhysicalMaterial> Material;
};

/** Game thread → physics thread, latest-wins */
struct FB1MudSimInput
{
	TArray<FB1MudSimParams> MudMaterials;

	/** cm; belly drag starts once the average sink depth passes it */
	float BellyClearance = 0.0f;

	/** Extra downhill pull on a mud wheel, as a fraction of gravity along the ground under it */
	float SlopePull = 0.0f;

	/** Ground normal Z below which mud counts as a slope */
	float SlopeUpZ = 1.0f;

	/** One-shot sink reset: the sim clears SinkDepth/SinkOffset once per new value. A counter, since input is latest-wins */
	uint32 SinkResetSerial = 0;

	/** N, world, at the centre of mass. B1Unstuck's push window */
	FVector UnstuckAssistForce = FVector::ZeroVector;
};

/** Physics thread → game thread, latest step */
struct FB1MudSimOutput
{
	/** Per wheel, cm */
	TArray<float> SinkDepth;

	/** Per wheel, mud friction curve value; 1 off mud */
	TArray<float> MudFrictionScale;

	/** Per wheel, N */
	TArray<float> RollingResistance;

	/** Per wheel, N */
	TArray<float> SlopePull;

	/** Per mud wheel, N along the wheel forward: tire + gravity + external force + slope pull, what rolling resistance caps against */
	TArray<float> NetPush;

	/** Per wheel, cm/s, |angular velocity × radius|: how fast the tread surface moves, slip included */
	TArray<float> TreadSpeed;

	/** cm the average sink depth is past belly clearance */
	float BellyDepth = 0.0f;

	/** N */
	float BellyDrag = 0.0f;

	/** Lowest ground normal Z under the wheels on mud; 1 off mud */
	float MudGroundUpZ = 1.0f;

	/** Last FB1MudSimInput::SinkResetSerial the sim applied */
	uint32 SinkResetSerial = 0;
};
