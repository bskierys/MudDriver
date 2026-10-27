#pragma once

#include "CoreMinimal.h"

class UCanvas;
class UB1MudVehicleComponent;
class AB1MudPatch;
class FDebugDisplayInfo;

namespace B1MudShowDebug
{
	/** `ShowDebug B1Mud` */
	extern const FName Category;

	/** Patches farther than this from the camera are skipped */
	constexpr float PatchDrawDistance = 5000.0f;

	/** Per-wheel mud state, belly and the InMud tag, drawn below the B1Vehicle text so both can be on */
	void DrawVehicle(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const UB1MudVehicleComponent& Mud);

	/** Preset, max depth, hard bottom and ramps above the patch centre */
	void DrawPatch(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const AB1MudPatch& Patch);
}
