// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MudDriverPawn.h"
#include "MudDriverSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class AMudDriverSportsCar : public AMudDriverPawn
{
	GENERATED_BODY()
	
public:

	AMudDriverSportsCar(const FObjectInitializer& ObjectInitializer);
};
