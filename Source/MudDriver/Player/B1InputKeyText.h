#pragma once

#include "CoreMinimal.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;

namespace B1InputKeyText
{
	/**
	 * "E / X": up to KeysPerDevice keyboard/mouse keys, then pad keys, mapped to the actions in the applied contexts.
	 * Pad buttons use short Xbox names (A, RT, LS, D-pad); "unbound" when nothing is mapped.
	 */
	FText GetKeyText(const UEnhancedInputLocalPlayerSubsystem& Subsystem, TConstArrayView<const UInputAction*> Actions, int32 KeysPerDevice = 1);
}
