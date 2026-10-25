// Copyright Epic Games, Inc. All Rights Reserved.

#include "MudDriverGameMode.h"
#include "MudDriverPlayerController.h"

AMudDriverGameMode::AMudDriverGameMode()
{
	PlayerControllerClass = AMudDriverPlayerController::StaticClass();
}
