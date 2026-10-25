// Copyright Epic Games, Inc. All Rights Reserved.

#include "MudDriverWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UMudDriverWheelRear::UMudDriverWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}