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

	UFUNCTION(Exec)
	void B1DamageVehicle(float Amount);

	UFUNCTION(Exec)
	void B1SetCondition(float Value);

	/** Negative drains */
	UFUNCTION(Exec)
	void B1AddFuel(float Litres);

	/** Lifts the jeep level above the mud under it and clears its sink state */
	UFUNCTION(Exec)
	void B1Unstuck();

	/** Hooks the nearest winch anchor within cable range; rest length = current distance */
	UFUNCTION(Exec)
	void B1WinchAttachNearest();

	UFUNCTION(Exec)
	void B1WinchDetach();

	/** Every fallen log back to its placed pose, frozen */
	UFUNCTION(Exec)
	void B1LogReset();

	/** Spawns the game mode's CharacterClass in front of the current pawn and possesses it */
	UFUNCTION(Exec)
	void B1SpawnCharacter();

	/** Same path as the interact key in the jeep */
	UFUNCTION(Exec)
	void B1ExitVehicle();

	/** Enters the nearest free jeep, at any distance */
	UFUNCTION(Exec)
	void B1EnterVehicle();

private:

	class UB1WinchComponent* GetPlayerWinch(const TCHAR* CheatName) const;

	class UB1AbilitySystemComponent* GetPlayerASC(const TCHAR* CheatName) const;
};
