#include "B1FuelConsumptionComponent.h"
#include "B1FuelConsumptionData.h"
#include "B1EngineLoadSource.h"
#include "B1FuelLog.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"

namespace
{
	constexpr float FuelWindowSeconds = 5.0f;
}

UB1FuelConsumptionComponent::UB1FuelConsumptionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UB1FuelConsumptionComponent::BeginPlay()
{
	Super::BeginPlay();

	EngineSource = Cast<IB1EngineLoadSource>(GetOwner()->FindComponentByInterface(UB1EngineLoadSource::StaticClass()));
	for (UActorComponent* Component : GetOwner()->GetComponentsByInterface(UB1FuelLoadSource::StaticClass()))
	{
		LoadSources.Add(Cast<IB1FuelLoadSource>(Component));
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UB1FuelConsumptionComponent::OnControllerChanged);
	}

	if (!Data || !DrainEffect || !FuelAttribute.IsValid())
	{
		UE_LOG(LogB1Fuel, Warning, TEXT("%s: fuel consumption off (Data %s, DrainEffect %s, FuelAttribute %s)"), *GetNameSafe(GetOwner()),
			*GetNameSafe(Data), *GetNameSafe(DrainEffect), *FuelAttribute.GetName());
	}
}

void UB1FuelConsumptionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void UB1FuelConsumptionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!Pawn || !Pawn->GetController() || !ASC)
	{
		FuelRate = 0.0f;
		EnginePower = 0.0f;
		Loads.Reset();
		return;
	}

	if (ASC != BoundAbilitySystem.Get())
	{
		BindAbilitySystem(ASC);
	}
	ConsumeFuel(DeltaTime, *ASC);
}

void UB1FuelConsumptionComponent::ConsumeFuel(float DeltaTime, UAbilitySystemComponent& ASC)
{
	Loads.Reset();
	if (!Data || !DrainEffect || bOutOfFuel || (EngineSource && !EngineSource->IsEngineRunning()))
	{
		FuelRate = 0.0f;
		EnginePower = 0.0f;
		return;
	}

	EnginePower = EngineSource ? EngineSource->GetNormalizedEnginePower() : 0.0f;
	FuelRate = Data->IdleRate + Data->WorkRate * EnginePower;
	for (const IB1FuelLoadSource* Source : LoadSources)
	{
		Source->AddFuelLoads(Loads);
	}
	for (const FB1FuelLoad& Load : Loads)
	{
		FuelRate += Data->LoadRates.FindRef(Load.Name) * Load.Load;
	}
	PendingDrain += FuelRate / 3600.0f * DeltaTime;

	DrainTimer += DeltaTime;
	if (DrainTimer < Data->DrainInterval)
	{
		return;
	}

	const FVector Location = GetOwner()->GetActorLocation();
	FuelWindow.Add(FVector2f(PendingDrain, FVector::Dist(Location, LastDrainLocation) / 100.0f));
	const int32 WindowSize = FMath::CeilToInt(FuelWindowSeconds / Data->DrainInterval);
	if (FuelWindow.Num() > WindowSize)
	{
		FuelWindow.RemoveAt(0, FuelWindow.Num() - WindowSize);
	}
	LastDrainLocation = Location;

	ApplyDrain(ASC, -PendingDrain);
	PendingDrain = 0.0f;
	DrainTimer = 0.0f;
}

void UB1FuelConsumptionComponent::ApplyDrain(UAbilitySystemComponent& ASC, float Litres) const
{
	const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(DrainEffect, 1.0f, ASC.MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(DrainSetByCallerTag, Litres);
		ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UB1FuelConsumptionComponent::BindAbilitySystem(UAbilitySystemComponent* ASC)
{
	UnbindAbilitySystem();

	BoundAbilitySystem = ASC;
	if (FuelAttribute.IsValid())
	{
		FuelChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(FuelAttribute).AddUObject(this, &UB1FuelConsumptionComponent::OnFuelChanged);
		UpdateOutOfFuel(ASC->GetNumericAttribute(FuelAttribute));
	}
	LastDrainLocation = GetOwner()->GetActorLocation();
	FuelWindow.Reset();
}

void UB1FuelConsumptionComponent::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get(); ASC && FuelAttribute.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(FuelAttribute).Remove(FuelChangedHandle);
	}
	BoundAbilitySystem.Reset();
}

void UB1FuelConsumptionComponent::OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	// The next tick binds the new driver's ASC; the pawn may only set it up after this notification.
	UnbindAbilitySystem();
}

void UB1FuelConsumptionComponent::OnFuelChanged(const FOnAttributeChangeData& Change)
{
	UpdateOutOfFuel(Change.NewValue);
}

void UB1FuelConsumptionComponent::UpdateOutOfFuel(float Fuel)
{
	const bool bEmpty = Fuel <= 0.0f;
	if (bEmpty == bOutOfFuel)
	{
		return;
	}

	bOutOfFuel = bEmpty;
	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get(); ASC && OutOfFuelTag.IsValid())
	{
		ASC->SetLooseGameplayTagCount(OutOfFuelTag, bOutOfFuel ? 1 : 0);
	}
	UE_LOG(LogB1Fuel, Log, TEXT("%s %s"), *GetNameSafe(GetOwner()), bOutOfFuel ? TEXT("out of fuel") : TEXT("refuelled"));
	OnOutOfFuelChanged.Broadcast(bOutOfFuel);
}

float UB1FuelConsumptionComponent::GetFuelPer100m() const
{
	FVector2f Sum = FVector2f::ZeroVector;
	for (const FVector2f& Sample : FuelWindow)
	{
		Sum += Sample;
	}
	return Sum.Y >= 1.0f ? Sum.X / Sum.Y * 100.0f : -1.0f;
}

float UB1FuelConsumptionComponent::GetLoad(FName Name) const
{
	const FB1FuelLoad* Load = Loads.FindByPredicate([Name](const FB1FuelLoad& Entry) { return Entry.Name == Name; });
	return Load ? Load->Load : 0.0f;
}
