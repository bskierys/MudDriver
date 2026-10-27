#pragma once

#include "CoreMinimal.h"

class UCanvas;
class UB1WinchComponent;
class FDebugDisplayInfo;

namespace B1WinchShowDebug
{
	/** `ShowDebug B1Winch` */
	extern const FName Category;

	/** Anchors farther than this from the winch are skipped */
	constexpr float AnchorDrawDistance = 5000.0f;

	/** Tension, length and pull above the winch point; with bDrawAnchors, each nearby anchor's distance and reach */
	void Draw(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const UB1WinchComponent& Winch, bool bDrawAnchors);
}
