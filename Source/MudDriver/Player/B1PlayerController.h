#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MudDriverPlayerController.h"
#include "B1PlayerController.generated.h"

class UB1InputConfig;
class AB1Character;
class AB1Jeep;

// Moves the PlayerState ASC avatar to whatever pawn is possessed (ADR-004).
UCLASS(abstract)
class MUDDRIVER_API AB1PlayerController : public AMudDriverPlayerController
{
	GENERATED_BODY()

public:
	/** Also feeds the vehicle HUD's fuel bar from the PlayerState ASC */
	virtual void Tick(float Delta) override;

	// Forwarded to the PlayerState ASC; public so gym rigs can press ability inputs.
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);

	/** The player's one on-foot character; seated in whatever jeep is possessed */
	AB1Character* GetOnFootCharacter() const { return OnFootCharacter; }

	/** Parks the possessed jeep and possesses the character at the driver door (or the first free fallback spot) */
	bool ExitVehicle();
	/** Possessing the jeep seats the character and unparks it */
	bool EnterVehicle(AB1Jeep* Jeep);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	/** Ability input actions, bound for both pawns (ADR-014) */
	UPROPERTY(EditDefaultsOnly, Category = "B1|Input")
	TObjectPtr<UB1InputConfig> InputConfig;

	/** Active only while a vehicle is possessed; DefaultMappingContexts stay on for every pawn */
	UPROPERTY(EditDefaultsOnly, Category = "B1|Input")
	TArray<TObjectPtr<UInputMappingContext>> VehicleMappingContexts;

	UPROPERTY(EditDefaultsOnly, Category = "B1|Input")
	TArray<TObjectPtr<UInputMappingContext>> OnFootMappingContexts;

private:
	void ApplyPawnMappingContexts(const APawn* InPawn);

	/** The character follows possession: seated in a possessed jeep, adopted when a character is possessed */
	void UpdateOnFootCharacter(APawn* InPawn);

	/** Capsule centre near the jeep with room for the capsule; falls back to above the door rather than fail */
	bool FindExitSpot(const AB1Jeep& Jeep, const AB1Character& Character, FVector& OutLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<AB1Character> OnFootCharacter;
};
