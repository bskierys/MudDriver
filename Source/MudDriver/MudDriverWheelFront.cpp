// Copyright Epic Games, Inc. All Rights Reserved.

#include "MudDriverWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UMudDriverWheelFront::UMudDriverWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}