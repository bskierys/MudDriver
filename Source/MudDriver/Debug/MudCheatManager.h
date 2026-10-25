#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "MudCheatManager.generated.h"

UCLASS()
class UMudCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

	/** Starts the bundled NetImguiServer; it auto-connects to the game/editor listening on localhost */
	UFUNCTION(Exec)
	void NetImGuiLaunch();
};
