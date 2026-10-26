#pragma once

#include "CoreMinimal.h"

namespace B1ImGui
{
	/** Condition, fuel and tags on the player's ASC, with refuel/damage/repair buttons */
	void DrawVehicleStatus(UWorld* World, bool* bOpen);
}
