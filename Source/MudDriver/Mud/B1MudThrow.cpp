#include "B1MudThrow.h"
#include "B1MudVehicleComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"

namespace B1MudThrowPrivate
{
	constexpr float CmPerM = 100.0f;
}

float B1MudThrow::GetMotion(const FWheelInput& Input, const FParams& Params)
{
	const float Tread = Params.FullTreadSpeed > 0.0f ? Input.TreadSpeed / Params.FullTreadSpeed : 0.0f;
	const float Slip = Params.FullSlipSpeed > 0.0f ? Input.SlipSpeed / Params.FullSlipSpeed : 0.0f;
	return FMath::Clamp(Tread + Slip, 0.0f, 1.0f);
}

float B1MudThrow::GetIntensity(const FWheelInput& Input, const FParams& Params)
{
	if (!Input.bOnMud)
	{
		return 0.0f;
	}

	const float DepthAlpha = Params.FullSinkDepth > 0.0f ? FMath::Clamp(Input.SinkDepth / Params.FullSinkDepth, 0.0f, 1.0f) : 1.0f;
	return FMath::Lerp(Params.MinDepthFactor, 1.0f, DepthAlpha) * GetMotion(Input, Params);
}

B1MudThrow::FWheelInput B1MudThrow::GetWheelInput(const UB1MudVehicleComponent& MudVehicle, int32 WheelIndex)
{
	FWheelInput Input;
	Input.bOnMud = MudVehicle.GetWheelMud(WheelIndex) != nullptr;
	Input.SinkDepth = MudVehicle.GetWheelSinkDepth(WheelIndex);
	Input.TreadSpeed = MudVehicle.GetWheelTreadSpeed(WheelIndex) / B1MudThrowPrivate::CmPerM;
	Input.SlipSpeed = FMath::Abs(MudVehicle.GetWheelState(WheelIndex).SlipMagnitude) / B1MudThrowPrivate::CmPerM;
	return Input;
}
