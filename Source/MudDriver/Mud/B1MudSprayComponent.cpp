#include "B1MudSprayComponent.h"
#include "B1MudVehicleComponent.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1MudLog.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "B1MudSettings.h"

namespace
{
	TAutoConsoleVariable<bool> CVarMudSpray(TEXT("b1.Mud.Spray"), true, TEXT("Niagara mud spray from spinning wheels"));

	const FName SpawnRateParam(TEXT("User.SpawnRate"));
	const FName SprayVelocityParam(TEXT("User.SprayVelocity"));
	const FName ColorParam(TEXT("User.Color"));

	/** cm clear of the mud surface and the tread, so depth-buffer collision doesn't catch particles at birth */
	constexpr float SurfaceLift = 20.0f;
}

UB1MudSprayComponent::UB1MudSprayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UB1MudSprayComponent::BeginPlay()
{
	Super::BeginPlay();

	MudVehicle = GetOwner()->FindComponentByClass<UB1MudVehicleComponent>();
	if (!SpraySystem)
	{
		SpraySystem = GetDefault<UB1MudSettings>()->SpraySystem.LoadSynchronous();
	}

	if (!SpraySystem)
	{
		UE_LOG(LogB1Mud, Warning, TEXT("%s: no SpraySystem, mud spray off"), *GetNameSafe(GetOwner()));
	}
}

void UB1MudSprayComponent::CreateWheelEmitters()
{
	USceneComponent* Root = GetOwner()->GetRootComponent();
	if (!SpraySystem || !Root)
	{
		return;
	}

	for (int32 WheelIndex = WheelEmitters.Num(); WheelIndex < MudVehicle->GetNumWheels(); ++WheelIndex)
	{
		UNiagaraComponent* Emitter = NewObject<UNiagaraComponent>(GetOwner(), *FString::Printf(TEXT("MudSpray%d"), WheelIndex));
		Emitter->SetAsset(SpraySystem);
		Emitter->SetupAttachment(Root);
		Emitter->SetVariableFloat(SpawnRateParam, 0.0f);
		Emitter->RegisterComponent();
		Emitter->Activate();
		WheelEmitters.Add(Emitter);
	}
	SpawnRates.Init(0.0f, WheelEmitters.Num());
}

void UB1MudSprayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MudVehicle)
	{
		return;
	}

	// Wheels are created with the physics state, which may come after BeginPlay.
	if (WheelEmitters.Num() < MudVehicle->GetNumWheels())
	{
		CreateWheelEmitters();
	}

	const FVector Forward = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	const bool bEnabled = CVarMudSpray.GetValueOnGameThread();

	for (int32 WheelIndex = 0; WheelIndex < WheelEmitters.Num() && WheelIndex < MudVehicle->GetNumWheels(); ++WheelIndex)
	{
		UNiagaraComponent* Emitter = WheelEmitters[WheelIndex];
		const FWheelStatus& Status = MudVehicle->GetWheelState(WheelIndex);
		const UB1PhysicalMaterial_Mud* Mud = MudVehicle->GetWheelMud(WheelIndex);
		// Same slip the sink model uses (cm/s). Forward drive torque spins the tread forward, so mud flies backward.
		const float SlipSpeed = FMath::Abs(Status.SlipMagnitude) * (Status.DriveTorque >= 0.0f ? 1.0f : -1.0f);
		const float ExcessSlip = FMath::Abs(SlipSpeed) / 100.0f - MinSlipSpeed;
		const bool bSpray = bEnabled && Mud && MudVehicle->GetWheelSinkDepth(WheelIndex) > 0.0f && ExcessSlip > 0.0f;

		SpawnRates[WheelIndex] = bSpray ? FMath::Min(ExcessSlip * SpawnRatePerSlip, MaxSpawnRate) : 0.0f;
		Emitter->SetVariableFloat(SpawnRateParam, SpawnRates[WheelIndex]);
		if (!bSpray)
		{
			continue;
		}

		const float Speed = FMath::Clamp(MinSpraySpeed + FMath::Abs(SlipSpeed) * SpraySpeedPerSlip, MinSpraySpeed, MaxSpraySpeed);
		const FVector Back = -Forward * FMath::Sign(SlipSpeed);
		const float Elevation = FMath::DegreesToRadians(SprayElevation);
		const FVector WorldVelocity = (Back * FMath::Cos(Elevation) + FVector::UpVector * FMath::Sin(Elevation)) * Speed;

		// The hit is pushed down by the sink (ADR-012). Spawn above the visible mud surface and behind the tread,
		// where the depth buffer shows neither tire nor ground, or depth-buffer collision pins the particles.
		const float Radius = MudVehicle->GetWheelRadius(WheelIndex);
		const FVector Surface = Status.HitLocation + FVector::UpVector * MudVehicle->GetWheelSinkDepth(WheelIndex);
		Emitter->SetWorldLocation(Surface + FVector::UpVector * SurfaceLift + Back * (Radius + SurfaceLift));

		// The emitter's cone axis is in system space, which follows the attached component's rotation.
		Emitter->SetVariableVec3(SprayVelocityParam, Emitter->GetComponentTransform().InverseTransformVectorNoScale(WorldVelocity));
		Emitter->SetVariableLinearColor(ColorParam, Mud->SprayColor);
	}
}
