#pragma once

#include "CoreMinimal.h"
#include "OffroadGameMode.h"
#include "B1GameMode.generated.h"

class AB1Character;

// PlayerControllerClass and DefaultPawnClass are set on the BP (BP_B1GameMode).
UCLASS(abstract)
class MUDDRIVER_API AB1GameMode : public AOffroadGameMode
{
	GENERATED_BODY()

public:
	AB1GameMode();

	/** On-foot avatar */
	UPROPERTY(EditDefaultsOnly, Category = "B1|Classes")
	TSubclassOf<AB1Character> CharacterClass;
};
