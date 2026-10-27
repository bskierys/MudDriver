// Copyright Epic Games, Inc. All Rights Reserved.


#include "MudDriverUI.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UMudDriverUI::UpdateSpeed(float NewSpeed)
{
	// format the speed to KPH or MPH
	float FormattedSpeed = FMath::Abs(NewSpeed) * (bIsMPH ? 0.022f : 0.036f);

	// call the Blueprint handler
	OnSpeedUpdate(FormattedSpeed);
}

void UMudDriverUI::UpdateGear(int32 NewGear)
{
	// call the Blueprint handler
	OnGearUpdate(NewGear);
}

void UMudDriverUI::UpdateFuel(float Fuel, float MaxFuel)
{
	const float Fraction = MaxFuel > 0.0f ? FMath::Clamp(Fuel / MaxFuel, 0.0f, 1.0f) : 0.0f;

	if (FuelBar)
	{
		// Hue 120° (green) at full down to 0° (red) at FuelLowFraction, through yellow.
		const float Health = FMath::GetRangePct(FuelLowFraction, 1.0f, Fraction);
		FuelBar->SetPercent(Fraction);
		FuelBar->SetFillColorAndOpacity(FLinearColor::MakeFromHSV8(static_cast<uint8>(FMath::Clamp(Health, 0.0f, 1.0f) * 85.0f), 230, 230));
	}

	if (FuelText)
	{
		FuelText->SetText(FText::FromString(FString::Printf(TEXT("%.0f/%.0f L"), Fuel, MaxFuel)));
	}
}