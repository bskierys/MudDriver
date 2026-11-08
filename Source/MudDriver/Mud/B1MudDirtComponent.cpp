#include "B1MudDirtComponent.h"
#include "B1MudVehicleComponent.h"
#include "B1MudThrow.h"
#include "B1MudLog.h"
#include "Components/PrimitiveComponent.h"

namespace
{
	constexpr float MinCPDChange = 0.005f;

	/** Matching radius when the wheel doesn't report one yet */
	constexpr float FallbackWheelRadius = 50.0f;
}

UB1MudDirtComponent::UB1MudDirtComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	bTickInEditor = true;
	WheelDirt.Init(0.0f, NumWheelSlots);
	TireCoat.Init(0.0f, NumWheelSlots);
	TireWetness.Init(0.0f, NumWheelSlots);
}

void UB1MudDirtComponent::OnRegister()
{
	Super::OnRegister();
	ApplyPreviewToTagged();
}

void UB1MudDirtComponent::BeginPlay()
{
	Super::BeginPlay();
	MudVehicle = GetOwner()->FindComponentByClass<UB1MudVehicleComponent>();
	if (!MudVehicle)
	{
		UE_LOG(LogB1Mud, Warning, TEXT("%s: no UB1MudVehicleComponent, dirt targets stay preview-only"), *GetNameSafe(GetOwner()));
	}
}

#if WITH_EDITOR
void UB1MudDirtComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (bTargetsResolved)
	{
		WriteTargets();
	}
	else
	{
		ApplyPreviewToTagged();
	}
}
#endif

void UB1MudDirtComponent::SetPreviewDirt(float Value)
{
	PreviewDirt = Value < 0.0f ? -1.0f : FMath::Min(Value, 1.0f);
	if (bTargetsResolved)
	{
		WriteTargets();
	}
	else
	{
		ApplyPreviewToTagged();
	}
}

void UB1MudDirtComponent::SetDirt(float Value)
{
	const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
	for (int32 WheelIndex = 0; WheelIndex < NumWheelSlots; ++WheelIndex)
	{
		WheelDirt[WheelIndex] = Clamped;
		TireCoat[WheelIndex] = Clamped;
		TireWetness[WheelIndex] = Clamped;
	}
	BodyDirt = Clamped;
	LoggedTenth = FMath::FloorToInt32(Clamped * 10.0f + KINDA_SMALL_NUMBER);
	UE_LOG(LogB1Mud, Log, TEXT("%s dirt set to %.2f"), *GetNameSafe(GetOwner()), Clamped);
	SetPreviewDirt(-1.0f);
}

float UB1MudDirtComponent::GetShownWheelDirt(int32 WheelIndex) const
{
	return PreviewDirt >= 0.0f ? PreviewDirt : GetWheelDirt(WheelIndex);
}

float UB1MudDirtComponent::GetShownBodyDirt() const
{
	return PreviewDirt >= 0.0f ? PreviewDirt : BodyDirt;
}

int32 UB1MudDirtComponent::GetNumTireTargets() const
{
	return Targets.FilterByPredicate([](const FTarget& Target) { return Target.Wheel != INDEX_NONE; }).Num();
}

int32 UB1MudDirtComponent::GetNumBodyTargets() const
{
	return Targets.FilterByPredicate([](const FTarget& Target) { return Target.Wheel == INDEX_NONE; }).Num();
}

void UB1MudDirtComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		ApplyPreviewToTagged();
		return;
	}

	Accumulate(DeltaTime);

	if (!bTargetsResolved && !ResolveTargets())
	{
		return;
	}
	WriteTargets();
}

void UB1MudDirtComponent::Accumulate(float DeltaTime)
{
	const int32 NumWheels = MudVehicle ? FMath::Min(MudVehicle->GetNumWheels(), NumWheelSlots) : 0;
	WheelIntensity.SetNumZeroed(NumWheels);
	if (NumWheels == 0)
	{
		return;
	}

	float IntensitySum = 0.0f;
	float MaxMotion = 0.0f;
	for (int32 WheelIndex = 0; WheelIndex < NumWheels; ++WheelIndex)
	{
		const B1MudThrow::FWheelInput Input = B1MudThrow::GetWheelInput(*MudVehicle, WheelIndex);
		WheelIntensity[WheelIndex] = B1MudThrow::GetIntensity(Input);
		IntensitySum += WheelIntensity[WheelIndex];
		if (Input.bOnMud)
		{
			MaxMotion = FMath::Max(MaxMotion, B1MudThrow::GetMotion(Input));
		}
		WheelDirt[WheelIndex] = FMath::Min(WheelDirt[WheelIndex] + WheelIntensity[WheelIndex] * DeltaTime / WheelSecondsToFull, 1.0f);
		UpdateTireLayer(WheelIndex, Input.bOnMud, DeltaTime);
	}

	const float Belly = FMath::Clamp(MudVehicle->GetBellyDepth() / BellyFullDepth, 0.0f, 1.0f) * BellyBodyRate * MaxMotion;
	BodyDirt = FMath::Min(BodyDirt + (IntensitySum / NumWheels + Belly) * DeltaTime / BodySecondsToFull, 1.0f);

	LogProgress();
}

void UB1MudDirtComponent::UpdateTireLayer(int32 WheelIndex, bool bOnMud, float DeltaTime)
{
	// Tread distance, so a spinning wheel coats (and sheds) faster than one rolling at the same ground speed.
	const float Rolled = MudVehicle->GetWheelTreadSpeed(WheelIndex) * DeltaTime;
	float& Coat = TireCoat[WheelIndex];
	float& Wetness = TireWetness[WheelIndex];

	if (!bOnMud)
	{
		Coat = FMath::Max(Coat - Rolled / (TireShedDistance * 100.0f), 0.0f);
		Wetness = FMath::Max(Wetness - DeltaTime / TireDrySeconds, 0.0f);
		return;
	}

	Wetness = 1.0f;
	if (Coat < TreadCoat)
	{
		const float Radius = MudVehicle->GetWheelRadius(WheelIndex) > 0.0f ? MudVehicle->GetWheelRadius(WheelIndex) : FallbackWheelRadius;
		Coat = FMath::Min(Coat + TreadCoat * Rolled / (UE_TWO_PI * Radius), TreadCoat);
	}
	else
	{
		Coat = FMath::Min(Coat + (1.0f - TreadCoat) * DeltaTime / TireCoatSecondsToFull, 1.0f);
	}
}

void UB1MudDirtComponent::LogProgress()
{
	float Highest = BodyDirt;
	for (const float Value : WheelDirt)
	{
		Highest = FMath::Max(Highest, Value);
	}

	const int32 Tenth = FMath::FloorToInt32(Highest * 10.0f + KINDA_SMALL_NUMBER);
	if (Tenth <= LoggedTenth)
	{
		return;
	}

	LoggedTenth = Tenth;
	UE_LOG(LogB1Mud, Log, TEXT("%s dirt w=%.2f/%.2f/%.2f/%.2f body=%.2f"), *GetNameSafe(GetOwner()),
		GetWheelDirt(0), GetWheelDirt(1), GetWheelDirt(2), GetWheelDirt(3), BodyDirt);
}

bool UB1MudDirtComponent::ResolveTargets()
{
	const int32 NumWheels = MudVehicle ? MudVehicle->GetNumWheels() : 0;
	if (NumWheels == 0)
	{
		return false;
	}

	TArray<UPrimitiveComponent*> Tagged;
	GetOwner()->GetComponents<UPrimitiveComponent>(Tagged);
	Tagged.RemoveAll([this](const UPrimitiveComponent* Component) { return !Component->ComponentHasTag(TargetTag); });

	Targets.Reset();
	TArray<FString> WheelNames;
	WheelNames.Init(TEXT("-"), NumWheels);
	FString BodyNames;
	for (UPrimitiveComponent* Component : Tagged)
	{
		FTarget& Target = Targets.AddDefaulted_GetRef();
		Target.Component = Component;

		float BestDistance = TNumericLimits<float>::Max();
		for (int32 WheelIndex = 0; WheelIndex < FMath::Min(NumWheels, NumWheelSlots); ++WheelIndex)
		{
			const float Radius = MudVehicle->GetWheelRadius(WheelIndex) > 0.0f ? MudVehicle->GetWheelRadius(WheelIndex) : FallbackWheelRadius;
			const float Distance = FVector::Dist(Component->GetComponentLocation(), MudVehicle->GetWheelLocation(WheelIndex));
			if (Distance <= Radius && Distance < BestDistance)
			{
				BestDistance = Distance;
				Target.Wheel = WheelIndex;
			}
		}

		if (Target.Wheel == INDEX_NONE)
		{
			BodyNames += (BodyNames.IsEmpty() ? TEXT("") : TEXT(",")) + Component->GetName();
		}
		else
		{
			WheelNames[Target.Wheel] = Component->GetName();
		}
	}

	FString Line = FString::Printf(TEXT("%s dirt targets: body=%s"), *GetNameSafe(GetOwner()), BodyNames.IsEmpty() ? TEXT("-") : *BodyNames);
	for (int32 WheelIndex = 0; WheelIndex < WheelNames.Num(); ++WheelIndex)
	{
		Line += FString::Printf(TEXT(" w%d=%s"), WheelIndex, *WheelNames[WheelIndex]);
	}
	UE_LOG(LogB1Mud, Log, TEXT("%s"), *Line);

	bTargetsResolved = true;
	return true;
}

void UB1MudDirtComponent::ApplyPreviewToTagged()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Nothing has accumulated outside a game, so preview off means clean.
	const float Value = FMath::Max(PreviewDirt, 0.0f);
	TArray<UPrimitiveComponent*> Tagged;
	Owner->GetComponents<UPrimitiveComponent>(Tagged);
	for (UPrimitiveComponent* Component : Tagged)
	{
		if (Component->ComponentHasTag(TargetTag))
		{
			for (int32 Slot = 0; Slot <= NumWheelSlots; ++Slot)
			{
				WriteSlot(*Component, Slot, Value);
			}
		}
	}
}

void UB1MudDirtComponent::WriteTargets()
{
	for (const FTarget& Target : Targets)
	{
		UPrimitiveComponent* Component = Target.Component.Get();
		if (!Component)
		{
			continue;
		}

		if (Target.Wheel != INDEX_NONE)
		{
			const bool bPreview = PreviewDirt >= 0.0f;
			WriteSlot(*Component, 0, GetShownWheelDirt(Target.Wheel));
			WriteSlot(*Component, 1, bPreview ? PreviewDirt : GetTireCoat(Target.Wheel));
			WriteSlot(*Component, 2, bPreview ? PreviewDirt : GetTireWetness(Target.Wheel));
			continue;
		}

		WriteSlot(*Component, 0, GetShownBodyDirt());
		for (int32 WheelIndex = 0; WheelIndex < NumWheelSlots; ++WheelIndex)
		{
			WriteSlot(*Component, 1 + WheelIndex, GetShownWheelDirt(WheelIndex));
		}
	}
}

void UB1MudDirtComponent::WriteSlot(UPrimitiveComponent& Component, int32 Slot, float Value)
{
	const TArray<float>& Data = Component.GetCustomPrimitiveData().Data;
	const float Current = Data.IsValidIndex(Slot) ? Data[Slot] : 0.0f;
	const float Change = FMath::Abs(Value - Current);
	// Ends of the range are written exactly, so a clean or capped jeep isn't left at 0.004 / 0.996.
	const bool bExactEnd = (Value == 0.0f || Value == 1.0f) && Change > 0.0f;
	if (Change >= MinCPDChange || bExactEnd || (!Data.IsValidIndex(Slot) && Value != 0.0f))
	{
		Component.SetCustomPrimitiveDataFloat(Slot, Value);
	}
}
