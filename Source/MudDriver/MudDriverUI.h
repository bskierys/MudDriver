// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MudDriverUI.generated.h"

class UProgressBar;
class UTextBlock;

/**
 *  Simple Vehicle HUD class
 *  Displays the current speed and gear.
 *  Widget setup is handled in a Blueprint subclass.
 */
UCLASS(abstract)
class UMudDriverUI : public UUserWidget
{
	GENERATED_BODY()
	
protected:

	/** Controls the display of speed in Km/h or MPH */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle")
	bool bIsMPH = false;

public:

	/** Called to update the speed display */
	void UpdateSpeed(float NewSpeed);

	/** Called to update the gear display */
	void UpdateGear(int32 NewGear);

	/** Fills FuelBar / FuelText if the Blueprint has them; the bar goes green (full) to red (FuelLowFraction) */
	void UpdateFuel(float Fuel, float MaxFuel);

protected:

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> FuelBar;

	/** "12/20 L" */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FuelText;

	/** Fuel fraction where the bar is fully red */
	UPROPERTY(EditAnywhere, Category = "Vehicle|Fuel", meta = (ClampMin = "0", ClampMax = "1"))
	float FuelLowFraction = 0.15f;


	/** Implemented in Blueprint to display the new speed */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnSpeedUpdate(float NewSpeed);

	/** Implemented in Blueprint to display the new gear */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnGearUpdate(int32 NewGear);
};
