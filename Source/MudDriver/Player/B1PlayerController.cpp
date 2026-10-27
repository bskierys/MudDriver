#include "B1PlayerController.h"
#include "B1PlayerState.h"
#include "B1Character.h"
#include "B1AbilitySystemComponent.h"
#include "B1GameplayTags.h"
#include "B1VehicleAttributeSet.h"
#include "B1InputConfig.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "MudDriverUI.h"
#include "B1Log.h"
#include "WheeledVehiclePawn.h"

void AB1PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ApplyPawnMappingContexts(InPawn);

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
	ASC->SetLooseGameplayTagCount(B1GameplayTags::State_Avatar_OnFoot, InPawn->IsA<AB1Character>() ? 1 : 0);
}

void AB1PlayerController::ApplyPawnMappingContexts(const APawn* InPawn)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalPlayerController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	if (!Subsystem)
	{
		return;
	}

	static const TArray<TObjectPtr<UInputMappingContext>> NoContexts;
	const bool bVehicle = InPawn && InPawn->IsA<AWheeledVehiclePawn>();
	const bool bOnFoot = InPawn && InPawn->IsA<AB1Character>();
	const TArray<TObjectPtr<UInputMappingContext>>& Active = bVehicle ? VehicleMappingContexts : bOnFoot ? OnFootMappingContexts : NoContexts;

	for (const TArray<TObjectPtr<UInputMappingContext>>* Set : { &VehicleMappingContexts, &OnFootMappingContexts })
	{
		for (UInputMappingContext* Context : *Set)
		{
			if (Context && !Active.Contains(Context))
			{
				Subsystem->RemoveMappingContext(Context);
			}
		}
	}
	for (UInputMappingContext* Context : Active)
	{
		if (Context && !Subsystem->HasMappingContext(Context))
		{
			Subsystem->AddMappingContext(Context, 0);
		}
	}

	UE_LOG(LogB1Vehicle, Log, TEXT("%s mapping contexts for %s: %s"), *GetName(), *GetNameSafe(InPawn),
		bVehicle ? TEXT("vehicle") : bOnFoot ? TEXT("on foot") : TEXT("none"));
}

void AB1PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!InputConfig || !EnhancedInput)
	{
		return;
	}

	for (const FB1InputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			EnhancedInput->BindAction(Action.InputAction, ETriggerEvent::Started, this, &ThisClass::AbilityInputTagPressed, Action.InputTag);
			EnhancedInput->BindAction(Action.InputAction, ETriggerEvent::Completed, this, &ThisClass::AbilityInputTagReleased, Action.InputTag);
			EnhancedInput->BindAction(Action.InputAction, ETriggerEvent::Canceled, this, &ThisClass::AbilityInputTagReleased, Action.InputTag);
		}
	}
}

void AB1PlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	const AB1PlayerState* B1PlayerState = GetPlayerState<AB1PlayerState>();
	if (UB1AbilitySystemComponent* ASC = B1PlayerState ? B1PlayerState->GetB1AbilitySystemComponent() : nullptr)
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void AB1PlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	const AB1PlayerState* B1PlayerState = GetPlayerState<AB1PlayerState>();
	if (UB1AbilitySystemComponent* ASC = B1PlayerState ? B1PlayerState->GetB1AbilitySystemComponent() : nullptr)
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

void AB1PlayerController::Tick(float Delta)
{
	Super::Tick(Delta);

	const AB1PlayerState* B1PlayerState = GetPlayerState<AB1PlayerState>();
	const UB1AbilitySystemComponent* ASC = B1PlayerState ? B1PlayerState->GetB1AbilitySystemComponent() : nullptr;
	if (IsValid(VehicleUI) && ASC)
	{
		VehicleUI->UpdateFuel(ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute()),
			ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetMaxFuelAttribute()));
	}
}
