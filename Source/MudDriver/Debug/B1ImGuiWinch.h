#pragma once

#include "CoreMinimal.h"

namespace B1ImGui
{
	/** Player vehicle's winch: anchor, length, tension, per-wheel push vs drag, live params with save to the vehicle BP */
	void DrawWinch(UWorld* World, bool* bOpen);
}
