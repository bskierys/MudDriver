#pragma once

#include "CoreMinimal.h"
#include "MudDriverPlayerController.h"
#include "B1PlayerController.generated.h"

// Moves the PlayerState ASC avatar to whatever pawn is possessed (ADR-004).
UCLASS(abstract)
class MUDDRIVER_API AB1PlayerController : public AMudDriverPlayerController
{
	GENERATED_BODY()

public:
	/** Also feeds the vehicle HUD's fuel bar from the PlayerState ASC */
	virtual void Tick(float Delta) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
};
