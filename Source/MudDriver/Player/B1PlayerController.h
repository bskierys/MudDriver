#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MudDriverPlayerController.h"
#include "B1PlayerController.generated.h"

class UB1InputConfig;

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
};
