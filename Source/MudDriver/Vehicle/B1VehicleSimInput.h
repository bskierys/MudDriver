#pragma once

#include "CoreMinimal.h"
#include "B1VehicleSimInput.generated.h"

namespace Chaos { class FSingleParticlePhysicsProxy; }

UENUM()
enum class EB1TestForce : uint8
{
	Off,
	Push,
	Spring
};

/** Stand-ins for the river current (push) and the winch (spring) used to test physics stepping */
USTRUCT()
struct FB1TestForceSettings
{
	GENERATED_BODY()

	/** World +Y */
	UPROPERTY(EditAnywhere, Category = "TestForce|Config", meta = (Units = "N"))
	float PushForce = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "TestForce|Config", meta = (Units = "m"))
	float SpringRestLength = 5.0f;

	/** N/m */
	UPROPERTY(EditAnywhere, Category = "TestForce|Config")
	float SpringStiffness = 20000.0f;

	/** N*s/m. Near critical for the 1500 kg jeep; lower values make it bounce on the cable */
	UPROPERTY(EditAnywhere, Category = "TestForce|Config")
	float SpringDamping = 9000.0f;

	UPROPERTY(EditAnywhere, Category = "TestForce|Config", meta = (Units = "N"))
	float SpringMaxForce = 15000.0f;
};

/** Winch cable state the physics thread needs (ADR-007). Lengths in cm, forces in N */
struct FB1WinchSimInput
{
	bool bAttached = false;
	FVector AnchorLocation = FVector::ZeroVector;

	/** Simulating anchor body (log); the sim reads its pose in-step and gives it the opposite force */
	Chaos::FSingleParticlePhysicsProxy* AnchorProxy = nullptr;
	/** In AnchorProxy's body frame */
	FVector AnchorPointLocal = FVector::ZeroVector;

	/** In the body's frame, so the sim moves it with the body each step */
	FVector WinchPointLocal = FVector::ZeroVector;

	/** Bumped on each attach; the sim resets its rest length to InitialRestLength once per new value */
	uint32 AttachSerial = 0;
	float InitialRestLength = 0.0f;

	/** [-1, 1]; negative reels out */
	float Pull = 0.0f;

	float MaxPullForce = 0.0f;
	float ReelInSpeed = 0.0f;
	float ReelOutSpeed = 0.0f;
	float MinRestLength = 0.0f;
	float MaxCableLength = 0.0f;
	/** N/m */
	float Stiffness = 0.0f;
	/** N*s/m */
	float Damping = 0.0f;

	/** Standstill hold while the handbrake is on, N cap */
	float HandbrakeHoldForce = 0.0f;
	/** cm/s; ground speed below which the hold engages */
	float HandbrakeHoldSpeed = 0.0f;
};

struct FB1WinchSimOutput
{
	float Tension = 0.0f;
	float RestLength = 0.0f;
	float Length = 0.0f;
	/** World, N, cable only */
	FVector Force = FVector::ZeroVector;
	/** Handbrake hold magnitude, N */
	float HoldForce = 0.0f;
	bool bHoldActive = false;
	uint32 AttachSerial = 0;
};

/** Game thread → physics thread data, copied once per game frame. Mud has its own (FB1MudSimInput) */
struct FB1VehicleSimInput
{
	EB1TestForce TestForce = EB1TestForce::Off;
	FB1TestForceSettings TestForceSettings;
	FVector TestForceAnchor = FVector::ZeroVector;

	FB1WinchSimInput Winch;
};
