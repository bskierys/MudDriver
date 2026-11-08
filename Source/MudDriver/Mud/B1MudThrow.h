#pragma once

#include "CoreMinimal.h"

class UB1MudVehicleComponent;

/** Thrown-mud intensity per wheel, shared by dirt and spray (ADR-021) */
namespace B1MudThrow
{
	struct FParams
	{
		/** cm of sink where depth stops adding mud */
		float FullSinkDepth = 15.0f;

		/** Depth factor for a wheel on mud with no sink (shallow mud still coats) */
		float MinDepthFactor = 0.3f;

		/** m/s of tread speed that alone gives full motion */
		float FullTreadSpeed = 5.0f;

		/** m/s of slip that alone gives full motion; spin throws more than rolling */
		float FullSlipSpeed = 3.0f;
	};

	struct FWheelInput
	{
		bool bOnMud = false;
		/** cm */
		float SinkDepth = 0.0f;
		/** m/s */
		float TreadSpeed = 0.0f;
		/** m/s */
		float SlipSpeed = 0.0f;
	};

	/** 0..1 from tread and slip speed, ignoring mud */
	MUDDRIVER_API float GetMotion(const FWheelInput& Input, const FParams& Params = FParams());

	/** 0..1; 0 off mud or for a still wheel */
	MUDDRIVER_API float GetIntensity(const FWheelInput& Input, const FParams& Params = FParams());

	MUDDRIVER_API FWheelInput GetWheelInput(const UB1MudVehicleComponent& MudVehicle, int32 WheelIndex);
}
