#include "B1MudVehicleComponent.h"
#include "B1MudSimHost.h"
#include "B1MudSimulation.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1MudSettings.h"
#include "B1MudLog.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

namespace
{
	/** cm, wheel bottoms above the mud surface after B1Unstuck */
	constexpr float UnstuckLift = 50.0f;
	constexpr float UnstuckTraceUp = 300.0f;
	constexpr float UnstuckTraceDown = 300.0f;

	const FWheelStatus NoWheel;
}

UB1MudVehicleComponent::UB1MudVehicleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UB1MudVehicleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UB1MudVehicleComponent::OnControllerChanged);
	}

	Host = Cast<IB1MudSimHost>(GetOwner()->FindComponentByInterface(UB1MudSimHost::StaticClass()));
	if (!Host)
	{
		UE_LOG(LogB1Mud, Warning, TEXT("%s: no IB1MudSimHost component, mud off"), *GetNameSafe(GetOwner()));
		return;
	}

	// Right after Chaos refreshes the wheel state and runs the vehicle's game-thread Update (both on PreTick), before the step.
	if (FPhysScene_Chaos* Scene = GetWorld()->GetPhysicsScene())
	{
		PhysSceneStepHandle = Scene->OnPhysSceneStep.AddUObject(this, &UB1MudVehicleComponent::OnPhysSceneStep);
	}
}

void UB1MudVehicleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetInMud(false);

	if (FPhysScene_Chaos* Scene = GetWorld()->GetPhysicsScene())
	{
		Scene->OnPhysSceneStep.Remove(PhysSceneStepHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UB1MudVehicleComponent::OnPhysSceneStep(FPhysScene_Chaos* Scene, float DeltaTime)
{
	FB1MudSimulation* Simulation = Host ? Host->GetMudSimulation() : nullptr;
	if (!Simulation)
	{
		return;
	}

	FB1MudSimInput Input;
	GatherMudParams(Input.MudMaterials);
	Input.BellyClearance = BellyClearance;
	Input.SinkResetSerial = SinkResetSerial;
	Input.UnstuckAssistForce = GetUnstuckAssistForce(DeltaTime);
	Simulation->SetGameThreadInput(Input);

	SimOutput = Simulation->GetGameThreadOutput();

	if (bLogSinkReset && SimOutput.SinkResetSerial == SinkResetSerial)
	{
		bLogSinkReset = false;
		TArray<FString> PerWheel;
		for (int32 WheelIndex = 0; WheelIndex < GetNumWheels(); ++WheelIndex)
		{
			PerWheel.Add(FString::Printf(TEXT("%.1f"), GetWheelSinkDepth(WheelIndex)));
		}
		UE_LOG(LogB1Mud, Display, TEXT("B1Unstuck %s: sim reset applied, sink=%s"), *GetNameSafe(GetOwner()), *FString::Join(PerWheel, TEXT("/")));
	}

	if (!bLoggedClearance)
	{
		LogMeasuredClearance();
	}
}

void UB1MudVehicleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateInMud(DeltaTime);
}

void UB1MudVehicleComponent::UpdateInMud(float DeltaTime)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->GetController() || !UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		return;
	}

	bool bMudContact = false;
	for (int32 WheelIndex = 0; WheelIndex < GetNumWheels() && !bMudContact; ++WheelIndex)
	{
		bMudContact = GetWheelMud(WheelIndex) != nullptr;
	}

	TimeSinceMudContact = bMudContact ? 0.0f : TimeSinceMudContact + DeltaTime;
	SetInMud(bMudContact || (bInMud && TimeSinceMudContact < MudTagReleaseDelay));
}

void UB1MudVehicleComponent::SetInMud(bool bNewInMud)
{
	if (bNewInMud == bInMud)
	{
		return;
	}

	bInMud = bNewInMud;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (ASC && InMudTag.IsValid())
	{
		ASC->SetLooseGameplayTagCount(InMudTag, bInMud ? 1 : 0);
	}
}

void UB1MudVehicleComponent::OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	SetInMud(false);
}

float UB1MudVehicleComponent::GetMudStrain() const
{
	const float Throttle = Host ? Host->GetMudEffectiveThrottle() : 0.0f;
	return Throttle * FMath::Clamp(GetAverageSinkDepth() / MudFullStrainDepth, 0.0f, 1.0f);
}

void UB1MudVehicleComponent::AddFuelLoads(TArray<FB1FuelLoad>& OutLoads) const
{
	OutLoads.Add({ FuelLoadName, GetMudStrain() });
}

int32 UB1MudVehicleComponent::GetNumWheels() const
{
	return Host ? Host->GetMudWheelCount() : 0;
}

const FWheelStatus& UB1MudVehicleComponent::GetWheelState(int32 WheelIndex) const
{
	return Host && WheelIndex < Host->GetMudWheelCount() ? Host->GetMudWheelState(WheelIndex) : NoWheel;
}

float UB1MudVehicleComponent::GetWheelRadius(int32 WheelIndex) const
{
	return Host ? Host->GetMudWheelRadius(WheelIndex) : 0.0f;
}

float UB1MudVehicleComponent::GetWheelWidth(int32 WheelIndex) const
{
	return Host ? Host->GetMudWheelWidth(WheelIndex) : 0.0f;
}

const UB1PhysicalMaterial_Mud* UB1MudVehicleComponent::GetWheelMud(int32 WheelIndex) const
{
	const FWheelStatus& Status = GetWheelState(WheelIndex);
	return Status.bInContact ? Cast<UB1PhysicalMaterial_Mud>(Status.PhysMaterial.Get()) : nullptr;
}

float UB1MudVehicleComponent::GetWheelSinkDepth(int32 WheelIndex) const
{
	return SimOutput.SinkDepth.IsValidIndex(WheelIndex) ? SimOutput.SinkDepth[WheelIndex] : 0.0f;
}

float UB1MudVehicleComponent::GetWheelMudFrictionScale(int32 WheelIndex) const
{
	return SimOutput.MudFrictionScale.IsValidIndex(WheelIndex) ? SimOutput.MudFrictionScale[WheelIndex] : 1.0f;
}

float UB1MudVehicleComponent::GetWheelRollingResistance(int32 WheelIndex) const
{
	return SimOutput.RollingResistance.IsValidIndex(WheelIndex) ? SimOutput.RollingResistance[WheelIndex] : 0.0f;
}

float UB1MudVehicleComponent::GetAverageSinkDepth() const
{
	const int32 NumWheels = GetNumWheels();
	float Sum = 0.0f;
	for (int32 WheelIndex = 0; WheelIndex < NumWheels; ++WheelIndex)
	{
		Sum += GetWheelSinkDepth(WheelIndex);
	}
	return NumWheels > 0 ? Sum / NumWheels : 0.0f;
}

void UB1MudVehicleComponent::Unstuck()
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (!Host || !Body)
	{
		return;
	}

	FRotator Rotation = Owner->GetActorRotation();
	Rotation.Pitch = 0.0f;
	Rotation.Roll = 0.0f;

	// Lowest wheel bottom relative to the actor, as if it were already level.
	float WheelBottom = GetNumWheels() > 0 ? MAX_flt : 0.0f;
	for (int32 WheelIndex = 0; WheelIndex < GetNumWheels(); ++WheelIndex)
	{
		const FVector Local = Owner->GetActorTransform().InverseTransformPosition(Host->GetMudWheelLocation(WheelIndex));
		WheelBottom = FMath::Min(WheelBottom, static_cast<float>(Local.Z) - GetWheelRadius(WheelIndex));
	}

	const FVector Location = Owner->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(B1Unstuck), false, Owner);
	FHitResult Hit;
	const bool bMud = GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, UnstuckTraceUp), Location - FVector(0, 0, UnstuckTraceDown),
		FCollisionObjectQueryParams(GetDefault<UB1MudSettings>()->MudObjectChannel.GetValue()), Params);

	FVector NewLocation = Location + FVector(0, 0, UnstuckLift);
	if (bMud)
	{
		NewLocation.Z = Hit.ImpactPoint.Z + UnstuckLift - WheelBottom;
	}

	Owner->SetActorLocationAndRotation(NewLocation, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	++SinkResetSerial;
	bLogSinkReset = true;
	UnstuckAssistTimeLeft = UnstuckAssistDuration;

	UE_LOG(LogB1Mud, Display, TEXT("B1Unstuck %s: %s, lifted %.0f cm to %s, sink before %.1f cm avg"), *GetNameSafe(Owner),
		bMud ? *FString::Printf(TEXT("mud surface Z=%.0f"), Hit.ImpactPoint.Z) : TEXT("no mud below"),
		NewLocation.Z - Location.Z, *NewLocation.ToCompactString(), GetAverageSinkDepth());
}

FVector UB1MudVehicleComponent::GetUnstuckAssistForce(float DeltaTime)
{
	if (UnstuckAssistTimeLeft <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	UnstuckAssistTimeLeft -= DeltaTime;
	if (UnstuckAssistTimeLeft <= 0.0f)
	{
		UE_LOG(LogB1Mud, Display, TEXT("B1Unstuck %s: assist over, sink %.1f cm avg"), *GetNameSafe(GetOwner()), GetAverageSinkDepth());
		return FVector::ZeroVector;
	}

	// Raw inputs: brake is reverse at standstill, and a fuel cut-off doesn't block the cheat.
	return GetOwner()->GetActorForwardVector().GetSafeNormal2D() * Host->GetMudDriveInput() * UnstuckAssistForce;
}

void UB1MudVehicleComponent::GatherMudParams(TArray<FB1MudSimParams>& OutParams)
{
	for (int32 WheelIndex = 0; WheelIndex < GetNumWheels(); ++WheelIndex)
	{
		if (UB1PhysicalMaterial_Mud* Mud = Cast<UB1PhysicalMaterial_Mud>(GetWheelState(WheelIndex).PhysMaterial.Get()))
		{
			KnownMudMaterials.AddUnique(Mud);
		}
	}

	OutParams.Reset(KnownMudMaterials.Num());
	for (UB1PhysicalMaterial_Mud* Mud : KnownMudMaterials)
	{
		FB1MudSimParams& Params = OutParams.AddDefaulted_GetRef();
		Params.Material = Mud;
		Params.MaxSinkDepth = Mud->MaxSinkDepth;
		Params.StaticSinkDepth = Mud->MaxSinkDepth * Mud->StaticSinkRatio;
		Params.SettleRate = Mud->SettleRate;
		Params.SinkRate = Mud->SinkRate;
		Params.RiseRate = Mud->RiseRate;
		Params.FullSinkSlipSpeed = Mud->FullSinkSlipSpeed;
		Params.FrictionVsDepth = *Mud->FrictionVsDepth.GetRichCurveConst();
		Params.RollingResistanceVsDepth = *Mud->RollingResistanceVsDepth.GetRichCurveConst();
		Params.SpeedDragCoefficient = Mud->SpeedDragCoefficient;
		Params.BellyDragCoefficient = Mud->BellyDragCoefficient;
	}
}

void UB1MudVehicleComponent::LogMeasuredClearance()
{
	const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	const FBodyInstance* BodyInstance = Body ? Body->GetBodyInstance() : nullptr;
	const float ForwardSpeed = Body ? FVector::DotProduct(Body->GetPhysicsLinearVelocity(), GetOwner()->GetActorForwardVector()) : 0.0f;
	if (!BodyInstance || GetNumWheels() == 0 || ForwardSpeed > 10.0f)
	{
		return;
	}

	float ContactZ = 0.0f;
	for (int32 WheelIndex = 0; WheelIndex < GetNumWheels(); ++WheelIndex)
	{
		const FWheelStatus& Wheel = GetWheelState(WheelIndex);
		if (!Wheel.bInContact || Cast<UB1PhysicalMaterial_Mud>(Wheel.PhysMaterial.Get()))
		{
			return;
		}
		ContactZ += Wheel.HitLocation.Z / GetNumWheels();
	}

	bLoggedClearance = true;
	UE_LOG(LogB1Mud, Log, TEXT("%s belly clearance: measured %.1f cm, BellyClearance %.1f cm"),
		*GetNameSafe(GetOwner()), BodyInstance->GetBodyBounds().Min.Z - ContactZ, BellyClearance);
}
