#include "B1Jeep.h"
#include "B1Character.h"
#include "B1VehicleMovementComponent.h"
#include "B1VehicleAttributeSet.h"
#include "B1GameplayTags.h"
#include "B1FuelConsumptionComponent.h"
#include "B1MudVehicleComponent.h"
#include "B1MudShowDebug.h"
#include "B1MudSprayComponent.h"
#include "B1MudTrailComponent.h"
#include "B1WinchComponent.h"
#include "B1WinchShowDebug.h"
#include "B1Log.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

AB1Jeep::AB1Jeep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UB1VehicleMovementComponent>(AWheeledVehiclePawn::VehicleMovementComponentName))
{
	MudVehicle = CreateDefaultSubobject<UB1MudVehicleComponent>(TEXT("MudVehicle"));
	MudVehicle->InMudTag = B1GameplayTags::Vehicle_State_InMud;
	MudSpray = CreateDefaultSubobject<UB1MudSprayComponent>(TEXT("MudSpray"));
	MudTrail = CreateDefaultSubobject<UB1MudTrailComponent>(TEXT("MudTrail"));

	Winch = CreateDefaultSubobject<UB1WinchComponent>(TEXT("Winch"));
	Winch->SetupAttachment(GetMesh());
	Winch->SetRelativeLocation(FVector(230.0f, 0.0f, 60.0f));
	Winch->WinchingTag = B1GameplayTags::Vehicle_State_Winching;

	DriverSeat = CreateDefaultSubobject<USceneComponent>(TEXT("DriverSeat"));
	DriverSeat->SetupAttachment(GetMesh());
	DriverSeat->SetRelativeLocation(FVector(-20.0f, -40.0f, 100.0f));

	DriverDoor = CreateDefaultSubobject<USceneComponent>(TEXT("DriverDoor"));
	DriverDoor->SetupAttachment(GetMesh());
	DriverDoor->SetRelativeLocation(FVector(-20.0f, -230.0f, 100.0f));

	// Data and DrainEffect are assets, set on BP_B1Jeep.
	FuelConsumption = CreateDefaultSubobject<UB1FuelConsumptionComponent>(TEXT("FuelConsumption"));
	FuelConsumption->FuelAttribute = UB1VehicleAttributeSet::GetFuelAttribute();
	FuelConsumption->DrainSetByCallerTag = B1GameplayTags::Data_Fuel;
	FuelConsumption->OutOfFuelTag = B1GameplayTags::Vehicle_State_OutOfFuel;
}

UB1VehicleMovementComponent* AB1Jeep::GetB1VehicleMovement() const
{
	return CastChecked<UB1VehicleMovementComponent>(GetVehicleMovementComponent());
}

UAbilitySystemComponent* AB1Jeep::GetAbilitySystemComponent() const
{
	return AbilitySystem.Get();
}

void AB1Jeep::BeginPlay()
{
	Super::BeginPlay();

	FuelConsumption->OnOutOfFuelChanged.AddUObject(this, &AB1Jeep::OnOutOfFuelChanged);
}

void AB1Jeep::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewController ? NewController->PlayerState : nullptr))
	{
		AbilitySystem = ASC;
	}

	UE_LOG(LogB1Vehicle, Log, TEXT("%s ASC link: %s"), *GetName(),
		*GetNameSafe(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(this)));

	SetParked(false);
}

void AB1Jeep::SetParked(bool bParked)
{
	UB1VehicleMovementComponent* Movement = GetB1VehicleMovement();
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::Vehicle_State_EngineOff, bParked ? 1 : 0);
	}
	if (bParked == IsParked())
	{
		return;
	}

	// Chaos parking applies the full handbrake torque to every wheel and needs no controller.
	if (bParked)
	{
		Movement->ClearDriverInput();
	}
	Movement->SetParked(bParked);
	Movement->SetEngineCut(EB1EngineCut::EngineOff, bParked);
	Movement->SetParkSleep(bParked);
	UE_LOG(LogB1Vehicle, Log, TEXT("%s %s"), *GetName(), bParked ? TEXT("parked, engine off") : TEXT("unparked, engine on"));
}

void AB1Jeep::GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const
{
	if (GetController())
	{
		return;
	}

	FB1InteractionOption& Option = Options.AddDefaulted_GetRef();
	Option.Text = NSLOCTEXT("B1Jeep", "Enter", "Enter");
	Option.TriggerEventTag = B1GameplayTags::Event_Interact_EnterVehicle;
	Option.Priority = 10;
	Option.InteractionPoint = DriverDoor->GetComponentLocation();

	// Same priority as Enter: the door and the winch are far apart, so the nearer one wins.
	const EB1WinchHook Hook = Winch->GetHookState();
	const bool bCarriedByCharacter = Hook == EB1WinchHook::Carried && Winch->GetHookCarrier() == &Character;
	if ((Hook == EB1WinchHook::Stowed && !UB1WinchComponent::FindCarriedBy(&Character)) || bCarriedByCharacter)
	{
		FB1InteractionOption& HookOption = Options.AddDefaulted_GetRef();
		HookOption.Text = bCarriedByCharacter ? NSLOCTEXT("B1Jeep", "ReturnHook", "Return hook") : NSLOCTEXT("B1Jeep", "TakeHook", "Take hook");
		HookOption.TriggerEventTag = bCarriedByCharacter ? B1GameplayTags::Event_Interact_ReturnHook : B1GameplayTags::Event_Interact_TakeHook;
		HookOption.Priority = 10;
		HookOption.InteractionPoint = Winch->GetComponentLocation();
	}
}

bool AB1Jeep::IsParked() const
{
	return GetB1VehicleMovement()->IsParked();
}

void AB1Jeep::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Continuous control, native like throttle (ADR-007, ADR-014).
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(WinchPullAction, ETriggerEvent::Triggered, this, &AB1Jeep::WinchPull);
		EnhancedInput->BindAction(WinchPullAction, ETriggerEvent::Completed, this, &AB1Jeep::WinchPull);
		EnhancedInput->BindAction(WinchDetachAction, ETriggerEvent::Started, this, &AB1Jeep::WinchDetach);
	}
}

void AB1Jeep::WinchPull(const FInputActionValue& Value)
{
	Winch->SetPull(Value.Get<float>());
}

void AB1Jeep::WinchDetach(const FInputActionValue& Value)
{
	Winch->Detach();
}

void AB1Jeep::OnOutOfFuelChanged(bool bOutOfFuel)
{
	GetB1VehicleMovement()->SetEngineCut(EB1EngineCut::OutOfFuel, bOutOfFuel);
	UE_LOG(LogB1Vehicle, Log, TEXT("%s %s"), *GetName(), bOutOfFuel ? TEXT("throttle cut") : TEXT("throttle back"));
}

void AB1Jeep::DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	Super::DrawShowDebug(HUD, Canvas, DisplayInfo, YL, YPos);

	B1MudShowDebug::DrawVehicle(Canvas, DisplayInfo, *MudVehicle);

	// Anchors only for the viewer's own vehicle, so rig jeeps don't draw them again.
	const bool bViewerVehicle = HUD && HUD->PlayerOwner && HUD->PlayerOwner->GetPawn() == this;
	B1WinchShowDebug::Draw(Canvas, DisplayInfo, *Winch, bViewerVehicle);
}

void AB1Jeep::UnPossessed()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::State_Avatar_InVehicle, 0);
	}

	Super::UnPossessed();
}
