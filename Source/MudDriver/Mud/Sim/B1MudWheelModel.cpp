#include "B1MudWheelModel.h"

namespace
{
	constexpr float MaxStopForceFraction = 0.4f;
	/** Rising is limited: cm/s at low speed, cm per cm rolled above it */
	constexpr float SinkOffsetMinRiseSpeed = 30.0f;
	constexpr float SinkOffsetRiseSlope = 0.5f;
}

namespace B1MudWheelModel
{
	float UpdateSinkDepth(float Depth, const FB1MudWheelParams& Params, const FB1MudSinkInput& Input)
	{
		const float Floor = FMath::Min(Params.StaticSinkDepth * Input.RelativeLoad, Params.MaxSinkDepth);
		const float Spin = FMath::Clamp(Input.SlipSpeed / Params.FullSinkSlipSpeed, 0.0f, 1.0f);
		const float RolledDistance = Input.RoadSpeed * Input.DeltaTime;

		if (Depth < Floor)
		{
			Depth = FMath::Min(Depth + Params.SettleRate * Input.DeltaTime, Floor);
		}
		Depth += Params.SinkRate * Spin * Input.RelativeLoad * Input.DeltaTime;
		if (Depth > Floor)
		{
			Depth = FMath::Max(Depth - Params.RiseRate * RolledDistance * (1.0f - Spin), Floor);
		}
		return FMath::Clamp(Depth, 0.0f, FMath::Min(Params.MaxSinkDepth, Input.HardGroundDepth));
	}

	float UpdateSinkOffset(float Offset, float TargetDepth, float RoadSpeed, float DeltaTime)
	{
		// It climbs out over distance, like out of a rut, so a fast exit doesn't drive sunk into firm ground.
		const float MaxRise = FMath::Max(SinkOffsetMinRiseSpeed, RoadSpeed * SinkOffsetRiseSlope) * DeltaTime;
		return FMath::Max(TargetDepth, Offset - MaxRise);
	}

	float GetRelativeLoad(float Load, float AverageLoad)
	{
		return AverageLoad > 0.0f ? FMath::Clamp(Load / AverageLoad, 0.0f, MaxLoadFactor) : 1.0f;
	}

	float GetNormalizedDepth(float Depth, const FB1MudWheelParams& Params)
	{
		return Params.MaxSinkDepth > 0.0f ? FMath::Clamp(Depth / Params.MaxSinkDepth, 0.0f, 1.0f) : 0.0f;
	}

	float GetFrictionScale(float Depth, const FB1MudWheelParams& Params)
	{
		return FMath::Max(Params.FrictionVsDepth.Eval(GetNormalizedDepth(Depth, Params), 1.0f), 0.0f);
	}

	float GetRollingResistanceCoefficient(float Depth, const FB1MudWheelParams& Params, float SpeedMps)
	{
		const float NormalizedDepth = GetNormalizedDepth(Depth, Params);
		return FMath::Max(Params.RollingResistanceVsDepth.Eval(NormalizedDepth, 0.0f), 0.0f)
			+ Params.SpeedDragCoefficient * NormalizedDepth * SpeedMps * SpeedMps;
	}

	float GetBellyDragLimit(float MeanCoefficient, float BellyDepth, float Mass)
	{
		return MeanCoefficient * BellyDepth / CmPerM * Mass * GravityCmPerS2;
	}

	float GetStopForce(float Mass, float Speed, float DeltaTime)
	{
		return Mass * Speed / DeltaTime * MaxStopForceFraction;
	}

	float LimitResistance(float Limit, float PushAlongMotion, float StopForce)
	{
		return FMath::Min(Limit, PushAlongMotion + StopForce);
	}
}
