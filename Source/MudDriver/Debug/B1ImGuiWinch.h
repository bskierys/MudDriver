#pragma once

#include "CoreMinimal.h"

namespace B1ImGui
{
	/** Player vehicle's winch, or the one whose hook the player carries: hook state, anchor, length, tension, push vs drag, live params with save to the vehicle BP */
	void DrawWinch(UWorld* World, bool* bOpen);
}
