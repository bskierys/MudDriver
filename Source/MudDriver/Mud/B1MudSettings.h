#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "B1MudSettings.generated.h"

class UNiagaraSystem;

/** Project Settings → Game → B1 Mud. Must match the channel and profile in DefaultEngine.ini (ADR-012, ADR-015) */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "B1 Mud"))
class MUDDRIVER_API UB1MudSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Object channel of mud surfaces; only wheel suspension traces block it */
	UPROPERTY(Config, EditAnywhere, Category = "Collision")
	TEnumAsByte<ECollisionChannel> MudObjectChannel = ECC_GameTraceChannel1;

	/** Collision profile of mud surfaces: object MudObjectChannel, QueryOnly, blocks only WorldDynamic */
	UPROPERTY(Config, EditAnywhere, Category = "Collision")
	FName SurfaceProfileName = TEXT("B1MudSurface");

	/** Default for UB1MudSprayComponent::SpraySystem */
	UPROPERTY(Config, EditAnywhere, Category = "Spray")
	TSoftObjectPtr<UNiagaraSystem> SpraySystem;
};
