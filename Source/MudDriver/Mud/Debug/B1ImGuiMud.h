#pragma once

#include "CoreMinimal.h"

namespace B1ImGui
{
	/** Per-wheel mud state of the player's vehicle, plus live editing of the loaded mud presets */
	void DrawMud(UWorld* World, bool* bOpen);
}
