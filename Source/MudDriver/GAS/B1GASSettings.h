#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "B1GASSettings.generated.h"

class UGameplayEffect;
class UB1AbilitySet;

// Project Settings → Game → B1 GAS. Effects used from code (cheats, jeep fuel drain).
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "B1 GAS"))
class MUDDRIVER_API UB1GASSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Instant, SetByCaller Data.Damage → Damage meta attribute.
	UPROPERTY(Config, EditAnywhere, Category = "Effects")
	TSoftClassPtr<UGameplayEffect> DamageEffect;

	// Instant, SetByCaller Data.Fuel (signed litres) → Fuel.
	UPROPERTY(Config, EditAnywhere, Category = "Effects")
	TSoftClassPtr<UGameplayEffect> FuelDrainEffect;

	UPROPERTY(Config, EditAnywhere, Category = "Effects")
	TSoftClassPtr<UGameplayEffect> FuelAddEffect;

	// Granted once to the PlayerState ASC (ADR-014).
	UPROPERTY(Config, EditAnywhere, Category = "Abilities")
	TArray<TSoftObjectPtr<UB1AbilitySet>> AbilitySets;
};
