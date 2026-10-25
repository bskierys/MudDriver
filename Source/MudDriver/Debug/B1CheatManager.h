#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "B1CheatManager.generated.h"

UCLASS()
class UB1CheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

	/** Starts the bundled NetImguiServer; it auto-connects to the game/editor listening on localhost */
	UFUNCTION(Exec)
	void NetImGuiLaunch();

	/** off | push | spring. Spring anchors at the jeep's current location */
	UFUNCTION(Exec)
	void B1TestForce(const FString& Mode);
};
