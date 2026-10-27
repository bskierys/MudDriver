#include "B1Jeep.h"
#include "B1VehicleMovementComponent.h"
#include "B1VehicleAttributeSet.h"
#include "B1GameplayTags.h"
#include "B1FuelConsumptionComponent.h"
#include "B1MudVehicleComponent.h"
#include "B1MudShowDebug.h"
#include "B1MudSprayComponent.h"
#include "B1MudTrailComponent.h"
#include "B1Log.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

AB1Jeep::AB1Jeep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UB1VehicleMovementComponent>(AWheeledVehiclePawn::VehicleMovementComponentName))
{
	MudVehicle = CreateDefaultSubobject<UB1MudVehicleComponent>(TEXT("MudVehicle"));
	MudVehicle->InMudTag = B1GameplayTags::Vehicle_State_InMud;
	MudSpray = CreateDefaultSubobject<UB1MudSprayComponent>(TEXT("MudSpray"));
	MudTrail = CreateDefaultSubobject<UB1MudTrailComponent>(TEXT("MudTrail"));

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
}

void AB1Jeep::OnOutOfFuelChanged(bool bOutOfFuel)
{
	GetB1VehicleMovement()->SetFuelCutOff(bOutOfFuel);
	UE_LOG(LogB1Vehicle, Log, TEXT("%s %s"), *GetName(), bOutOfFuel ? TEXT("throttle cut") : TEXT("throttle back"));
}

void AB1Jeep::DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	Super::DrawShowDebug(HUD, Canvas, DisplayInfo, YL, YPos);

	B1MudShowDebug::DrawVehicle(Canvas, DisplayInfo, *MudVehicle);
}

void AB1Jeep::UnPossessed()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::State_Avatar_InVehicle, 0);
	}

	Super::UnPossessed();
}
