#pragma once

#include "CoreMinimal.h"
#include "OffroadGameMode.h"
#include "B1GameMode.generated.h"

// PlayerControllerClass and DefaultPawnClass are set on the BP (BP_B1GameMode).
UCLASS(abstract)
class MUDDRIVER_API AB1GameMode : public AOffroadGameMode
{
	GENERATED_BODY()

public:
	AB1GameMode();
};
