#include "B1PlayerController.h"
#include "B1PlayerState.h"
#include "B1AbilitySystemComponent.h"
#include "B1GameplayTags.h"
#include "B1Log.h"
#include "WheeledVehiclePawn.h"

void AB1PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AB1PlayerState* B1PlayerState = GetPlayerState<AB1PlayerState>();
	UB1AbilitySystemComponent* ASC = B1PlayerState ? B1PlayerState->GetB1AbilitySystemComponent() : nullptr;
	if (!ASC || !InPawn)
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("%s possessed %s without a B1 PlayerState ASC"), *GetName(), *GetNameSafe(InPawn));
		return;
	}

	ASC->InitAbilityActorInfo(B1PlayerState, InPawn);

	const bool bVehicle = InPawn->IsA<AWheeledVehiclePawn>();
	ASC->SetLooseGameplayTagCount(B1GameplayTags::State_Avatar_InVehicle, bVehicle ? 1 : 0);
}
