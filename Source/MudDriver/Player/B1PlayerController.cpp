#include "B1PlayerController.h"
#include "B1PlayerState.h"
#include "B1Character.h"
#include "B1GameMode.h"
#include "B1Jeep.h"
#include "B1WinchComponent.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
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
	UpdateOnFootCharacter(InPawn);

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

void AB1PlayerController::UpdateOnFootCharacter(APawn* InPawn)
{
	if (AB1Character* PossessedCharacter = Cast<AB1Character>(InPawn))
	{
		// A character spawned elsewhere (cheat, rig) takes over; the hidden one in the seat goes.
		if (IsValid(OnFootCharacter) && OnFootCharacter != PossessedCharacter && OnFootCharacter->IsSeated())
		{
			OnFootCharacter->Destroy();
		}
		OnFootCharacter = PossessedCharacter;
		if (PossessedCharacter->IsSeated())
		{
			PossessedCharacter->LeaveSeat();
		}
		return;
	}

	AB1Jeep* Jeep = Cast<AB1Jeep>(InPawn);
	if (!Jeep)
	{
		return;
	}

	if (!IsValid(OnFootCharacter))
	{
		const AB1GameMode* GameMode = GetWorld()->GetAuthGameMode<AB1GameMode>();
		if (!GameMode || !GameMode->CharacterClass)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		OnFootCharacter = GetWorld()->SpawnActor<AB1Character>(GameMode->CharacterClass, Jeep->GetDriverSeat()->GetComponentTransform(), Params);
		if (!OnFootCharacter)
		{
			UE_LOG(LogB1Vehicle, Warning, TEXT("%s couldn't spawn the on-foot character"), *GetName());
			return;
		}
	}

	if (UB1WinchComponent* CarriedWinch = UB1WinchComponent::FindCarriedBy(OnFootCharacter))
	{
		CarriedWinch->Detach();
	}
	OnFootCharacter->SitIn(Jeep->GetDriverSeat());
	UE_LOG(LogB1Vehicle, Log, TEXT("%s seated in %s"), *OnFootCharacter->GetName(), *Jeep->GetName());
}

bool AB1PlayerController::ExitVehicle()
{
	AB1Jeep* Jeep = Cast<AB1Jeep>(GetPawn());
	AB1Character* ExitCharacter = OnFootCharacter;
	if (!Jeep || !IsValid(ExitCharacter) || !ExitCharacter->IsSeated())
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("%s ExitVehicle: needs a possessed AB1Jeep with the character seated"), *GetName());
		return false;
	}

	FVector Location;
	if (!FindExitSpot(*Jeep, *ExitCharacter, Location))
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("%s ExitVehicle: every exit spot around %s is blocked"), *GetName(), *Jeep->GetName());
		return false;
	}

	Jeep->SetParked(true);
	const FRotator Facing(0.0f, Jeep->GetActorRotation().Yaw, 0.0f);
	ExitCharacter->LeaveSeat();
	ExitCharacter->SetActorLocationAndRotation(Location, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	Possess(ExitCharacter);
	SetControlRotation(Facing);

	// ADR-004: vehicle tags stay on the player ASC; the jeep components drop theirs on the controller change.
	const AB1PlayerState* B1PlayerState = GetPlayerState<AB1PlayerState>();
	if (const UB1AbilitySystemComponent* ASC = B1PlayerState ? B1PlayerState->GetB1AbilitySystemComponent() : nullptr)
	{
		FGameplayTagContainer Owned;
		ASC->GetOwnedGameplayTags(Owned);
		UE_LOG(LogB1Vehicle, Log, TEXT("%s exited %s at %s; tags on foot: %s"), *GetName(), *Jeep->GetName(),
			*Location.ToCompactString(), *Owned.ToStringSimple());
	}
	return true;
}

bool AB1PlayerController::EnterVehicle(AB1Jeep* Jeep)
{
	if (!Jeep || !Cast<AB1Character>(GetPawn()) || Jeep->GetController())
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("%s EnterVehicle: needs the character possessed and a free jeep (%s)"), *GetName(), *GetNameSafe(Jeep));
		return false;
	}

	Possess(Jeep);
	UE_LOG(LogB1Vehicle, Log, TEXT("%s entered %s"), *GetName(), *Jeep->GetName());
	return true;
}

bool AB1PlayerController::FindExitSpot(const AB1Jeep& Jeep, const AB1Character& ExitCharacter, FVector& OutLocation) const
{
	const FTransform& JeepTransform = Jeep.GetActorTransform();
	const FVector Spots[] = {
		Jeep.GetDriverDoor()->GetComponentLocation(),
		JeepTransform.TransformPosition(FVector(-20.0f, 230.0f, 100.0f)),
		JeepTransform.TransformPosition(FVector(-350.0f, 0.0f, 100.0f)),
		JeepTransform.TransformPosition(FVector(380.0f, 0.0f, 100.0f)),
	};

	const UCapsuleComponent* Capsule = ExitCharacter.GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight);

	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(B1ExitGround), false, &ExitCharacter);
	GroundParams.AddIgnoredActor(&Jeep);
	FCollisionQueryParams RoomParams(SCENE_QUERY_STAT(B1ExitRoom), false, &ExitCharacter);

	// Starts high, so a sunk jeep or a slope doesn't start the trace inside a hard-bottom slab.
	// The Pawn channel ignores mud surfaces, so it lands on the hard bottom like the walking capsule.
	TArray<FVector, TInlineAllocator<4>> Grounds;
	for (const FVector& Spot : Spots)
	{
		FHitResult Ground;
		if (GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0.0f, 0.0f, 400.0f), Spot - FVector(0.0f, 0.0f, 600.0f), ECC_Pawn, GroundParams))
		{
			Grounds.Add(Ground.ImpactPoint);
		}
	}

	// Dropped a little above the ground; the character falls the rest.
	for (const float Lift : { 2.0f, 30.0f, 60.0f })
	{
		for (const FVector& Ground : Grounds)
		{
			const FVector Candidate = Ground + FVector(0.0f, 0.0f, HalfHeight + Lift);
			if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Shape, RoomParams))
			{
				OutLocation = Candidate;
				return true;
			}
		}
	}

	// Let the engine push the capsule out of whatever it overlaps.
	for (const FVector& Ground : Grounds)
	{
		FVector Candidate = Ground + FVector(0.0f, 0.0f, HalfHeight + 30.0f);
		if (GetWorld()->FindTeleportSpot(&ExitCharacter, Candidate, FRotator::ZeroRotator)
			&& !GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Shape, RoomParams))
		{
			OutLocation = Candidate;
			return true;
		}
	}

	// Walking is a prototype feature: never trap the player in the jeep.
	OutLocation = Spots[0] + FVector(0.0f, 0.0f, HalfHeight + 150.0f);
	UE_LOG(LogB1Vehicle, Warning, TEXT("no free exit spot around %s, dropping the character above the door"), *Jeep.GetName());
	return true;
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
