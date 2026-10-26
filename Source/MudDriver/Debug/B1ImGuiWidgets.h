#pragma once

#include "CoreMinimal.h"
#include "NetImguiModule.h"

#if NETIMGUI_ENABLED
namespace B1ImGui
{
	/** Width of the typed-value field next to a drag */
	static constexpr float TypeFieldWidth = 70.0f;

	/** Drag plus a text field for typing an exact value; both clamp to [Min, Max]. Use for every tunable float */
	bool DragFloatTyped(const char* Label, float* Value, float Speed, float Min, float Max, const char* Format = "%.2f");
}
#endif
