#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"

/** One mud surface's tuning as plain values (ADR-001). Curves take normalized depth */
struct FB1MudWheelParams
{
	float MaxSinkDepth = 0.0f;
	float StaticSinkDepth = 0.0f;
	float SettleRate = 0.0f;
	float SinkRate = 0.0f;
	float RiseRate = 0.0f;
	float FullSinkSlipSpeed = 1.0f;
	FRichCurve FrictionVsDepth;
	FRichCurve RollingResistanceVsDepth;
	float SpeedDragCoefficient = 0.0f;
	float BellyDragCoefficient = 0.0f;
};

/** One wheel on mud for one step */
struct FB1MudSinkInput
{
	/** m/s, absolute */
	float SlipSpeed = 0.0f;

	/** m/s, absolute */
	float RoadSpeed = 0.0f;

	/** Wheel load over the average load of the wheels in contact */
	float RelativeLoad = 1.0f;

	/** cm below the mud surface to the hard ground */
	float HardGroundDepth = MAX_flt;

	float DeltaTime = 0.0f;
};

/** Mud wheel physics without Chaos or UObjects. Depths in cm, forces in the caller's units */
namespace B1MudWheelModel
{
	constexpr float CmPerM = 100.0f;
	constexpr float GravityCmPerS2 = 980.665f;
	/** Chaos forces are in kg*cm/s^2 */
	constexpr float NewtonsToUnreal = 100.0f;

	/** Caps the load scaling of sinking, so a hard landing doesn't bury the wheel */
	constexpr float MaxLoadFactor = 1.5f;

	/** cm/s; below it resistance opposes the push, not the motion */
	constexpr float StandstillSpeed = 1.0f;

	/** Weight settles to a floor, wheelspin digs below it, rolling with grip climbs back to it */
	float UpdateSinkDepth(float Depth, const FB1MudWheelParams& Params, const FB1MudSinkInput& Input);

	/** Sink offset follows the depth down at once and rises at a limited rate, so leaving mud doesn't pop. RoadSpeed in cm/s */
	float UpdateSinkOffset(float Offset, float TargetDepth, float RoadSpeed, float DeltaTime);

	float GetRelativeLoad(float Load, float AverageLoad);

	float GetNormalizedDepth(float Depth, const FB1MudWheelParams& Params);

	/** Multiplier on the tire friction */
	float GetFrictionScale(float Depth, const FB1MudWheelParams& Params);

	/** Fraction of the wheel load. SpeedMps is the wheel's road speed */
	float GetRollingResistanceCoefficient(float Depth, const FB1MudWheelParams& Params, float SpeedMps);

	/** Drag limit for the belly. MeanCoefficient over the mud wheels, Mass in kg; result in kg*cm/s^2 */
	float GetBellyDragLimit(float MeanCoefficient, float BellyDepth, float Mass);

	/** Share of the stopping force resistance may use per step, as Chaos does for braking. Speed in cm/s */
	float GetStopForce(float Mass, float Speed, float DeltaTime);

	/** Resistance never reverses the motion: at most the push along the motion plus a share of the stopping force */
	float LimitResistance(float Limit, float PushAlongMotion, float StopForce);
}
