#pragma once

#include "CoreMinimal.h"

namespace B1ImGui
{
	/** Writes the asset's package to disk (editor builds only; false elsewhere) */
	bool SaveAssetPackage(UObject& Asset);
}
