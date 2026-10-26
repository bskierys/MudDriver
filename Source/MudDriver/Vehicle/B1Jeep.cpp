#include "B1Jeep.h"
#include "B1VehicleMovementComponent.h"
#include "B1VehicleTuningData.h"
#include "B1AbilitySystemComponent.h"
#include "B1VehicleAttributeSet.h"
#include "B1GameplayTags.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1MudShowDebug.h"
#include "B1MudSprayComponent.h"
#include "B1Log.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

namespace
{
	constexpr float FuelWindowSeconds = 5.0f;
	/** Keeps the tag through bumps where wheels briefly leave the mud */
	constexpr float MudTagReleaseDelay = 0.5f;
}

AB1Jeep::AB1Jeep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UB1VehicleMovementComponent>(AWheeledVehiclePawn::VehicleMovementComponentName))
{
	MudSpray = CreateDefaultSubobject<UB1MudSprayComponent>(TEXT("MudSpray"));
}

UB1VehicleMovementComponent* AB1Jeep::GetB1VehicleMovement() const
{
	return CastChecked<UB1VehicleMovementComponent>(GetVehicleMovementComponent());
}

UAbilitySystemComponent* AB1Jeep::GetAbilitySystemComponent() const
{
	return AbilitySystem.Get();
}

void AB1Jeep::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewController ? NewController->PlayerState : nullptr))
	{
		AbilitySystem = ASC;
		FuelChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UB1VehicleAttributeSet::GetFuelAttribute()).AddUObject(this, &AB1Jeep::OnFuelChanged);
		UpdateOutOfFuel(ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute()));
	}
	LastDrainLocation = GetActorLocation();
	FuelWindow.Reset();

	UE_LOG(LogB1Vehicle, Log, TEXT("%s ASC link: %s"), *GetName(),
		*GetNameSafe(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(this)));
}

void AB1Jeep::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ConsumeFuel(DeltaSeconds);
	UpdateInMud(DeltaSeconds);
}

void AB1Jeep::UpdateInMud(float DeltaSeconds)
{
	if (!Controller || !AbilitySystem.IsValid())
	{
		return;
	}

	const UB1VehicleMovementComponent* Movement = GetB1VehicleMovement();
	bool bMudContact = false;
	for (int32 WheelIndex = 0; WheelIndex < Movement->GetNumWheels() && !bMudContact; ++WheelIndex)
	{
		const FWheelStatus& Wheel = Movement->GetWheelState(WheelIndex);
		bMudContact = Wheel.bInContact && Cast<UB1PhysicalMaterial_Mud>(Wheel.PhysMaterial.Get());
	}

	TimeSinceMudContact = bMudContact ? 0.0f : TimeSinceMudContact + DeltaSeconds;
	SetInMud(bMudContact || (bInMud && TimeSinceMudContact < MudTagReleaseDelay));
}

void AB1Jeep::SetInMud(bool bNewInMud)
{
	if (bNewInMud == bInMud)
	{
		return;
	}

	bInMud = bNewInMud;
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::Vehicle_State_InMud, bInMud ? 1 : 0);
	}
}

void AB1Jeep::ConsumeFuel(float DeltaSeconds)
{
	UB1AbilitySystemComponent* ASC = Cast<UB1AbilitySystemComponent>(AbilitySystem.Get());
	if (!Controller || !ASC || !TuningData || GetB1VehicleMovement()->IsFuelCutOff())
	{
		FuelRate = 0.0f;
		MudStrain = 0.0f;
		return;
	}

	const UB1VehicleMovementComponent* Movement = GetB1VehicleMovement();
	MudStrain = Movement->GetEffectiveThrottle() * FMath::Clamp(Movement->GetAverageSinkDepth() / TuningData->MudFullStrainDepth, 0.0f, 1.0f);
	FuelRate = TuningData->IdleRate + TuningData->WorkRate * Movement->GetNormalizedEnginePower() + TuningData->MudWorkRate * MudStrain;
	PendingDrain += FuelRate / 3600.0f * DeltaSeconds;

	DrainTimer += DeltaSeconds;
	if (DrainTimer < TuningData->DrainInterval)
	{
		return;
	}

	const FVector Location = GetActorLocation();
	FuelWindow.Add(FVector2f(PendingDrain, FVector::Dist(Location, LastDrainLocation) / 100.0f));
	const int32 WindowSize = FMath::CeilToInt(FuelWindowSeconds / TuningData->DrainInterval);
	if (FuelWindow.Num() > WindowSize)
	{
		FuelWindow.RemoveAt(0, FuelWindow.Num() - WindowSize);
	}
	LastDrainLocation = Location;

	ASC->AddVehicleFuel(-PendingDrain);
	PendingDrain = 0.0f;
	DrainTimer = 0.0f;
}

void AB1Jeep::OnFuelChanged(const FOnAttributeChangeData& Data)
{
	UpdateOutOfFuel(Data.NewValue);
}

void AB1Jeep::UpdateOutOfFuel(float Fuel)
{
	const bool bEmpty = Fuel <= 0.0f;
	if (bEmpty == GetB1VehicleMovement()->IsFuelCutOff())
	{
		return;
	}

	GetB1VehicleMovement()->SetFuelCutOff(bEmpty);
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::Vehicle_State_OutOfFuel, bEmpty ? 1 : 0);
	}
	UE_LOG(LogB1Vehicle, Log, TEXT("%s %s"), *GetName(), bEmpty ? TEXT("out of fuel, throttle cut") : TEXT("refuelled, throttle back"));
}

float AB1Jeep::GetFuelPer100m() const
{
	FVector2f Sum = FVector2f::ZeroVector;
	for (const FVector2f& Sample : FuelWindow)
	{
		Sum += Sample;
	}
	return Sum.Y >= 1.0f ? Sum.X / Sum.Y * 100.0f : -1.0f;
}

void AB1Jeep::DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	Super::DrawShowDebug(HUD, Canvas, DisplayInfo, YL, YPos);

	B1MudShowDebug::DrawVehicle(Canvas, DisplayInfo, *GetB1VehicleMovement(), AbilitySystem.Get());
}

void AB1Jeep::UnPossessed()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::State_Avatar_InVehicle, 0);
		SetInMud(false);
		ASC->GetGameplayAttributeValueChangeDelegate(UB1VehicleAttributeSet::GetFuelAttribute()).Remove(FuelChangedHandle);
	}

	Super::UnPossessed();
}
