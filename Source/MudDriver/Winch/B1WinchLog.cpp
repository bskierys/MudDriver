#include "B1WinchLog.h"
#include "B1Log.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "B1WinchShowDebug.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Engine BasicShapes are 100 cm across */
	constexpr float UnitMeshSize = 100.0f;
}

AB1WinchLog::AB1WinchLog()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// Unscaled, so children and the anchor area get a clean frame; trunk along X, root end at -X.
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetSimulatePhysics(false);

	TrunkVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrunkVisual"));
	TrunkVisual->SetupAttachment(Body);
	TrunkVisual->SetStaticMesh(Cylinder.Object);
	TrunkVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrunkVisual->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	RootBall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RootBall"));
	RootBall->SetupAttachment(Body);
	RootBall->SetStaticMesh(Sphere.Object);
	RootBall->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	RootBall->SetSimulatePhysics(false);

	Anchor = CreateDefaultSubobject<UB1WinchAnchorComponent>(TEXT("Anchor"));
	Anchor->SetupAttachment(Body);

	ApplyShape();
}

void AB1WinchLog::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyShape();
}

void AB1WinchLog::ApplyShape()
{
	Body->SetBoxExtent(FVector(TrunkLength * 0.5f, TrunkRadius, TrunkRadius));
	TrunkVisual->SetRelativeScale3D(FVector(2.0f * TrunkRadius, 2.0f * TrunkRadius, TrunkLength) / UnitMeshSize);

	// Bottom flush with the box, so both rest on the ground.
	RootBall->SetRelativeLocation(FVector(-TrunkLength * 0.5f, 0.0f, RootBallRadius - TrunkRadius));
	RootBall->SetRelativeScale3D(FVector(2.0f * RootBallRadius / UnitMeshSize));

	Anchor->AreaStart = FVector(-TrunkLength * 0.4f, 0.0f, 0.0f);
	Anchor->AreaEnd = FVector(TrunkLength * 0.48f, 0.0f, 0.0f);
	Anchor->TrunkRadius = TrunkRadius;
}

void AB1WinchLog::BeginPlay()
{
	Super::BeginPlay();

	PlacedTransform = GetActorTransform();
	ApplyMass();
	ShowDebugHandle = AHUD::OnShowDebugInfo.AddUObject(this, &AB1WinchLog::DrawShowDebug);
}

void AB1WinchLog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AHUD::OnShowDebugInfo.Remove(ShowDebugHandle);

	Super::EndPlay(EndPlayReason);
}

void AB1WinchLog::ApplyMass()
{
	Body->SetMassOverrideInKg(NAME_None, TrunkMass, true);
	RootBall->SetMassOverrideInKg(NAME_None, RootBallMass, true);
}

void AB1WinchLog::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The winch's tension is a step old; a frame late on a threshold doesn't matter (ADR-018).
	const float Tension = GetTension();
	if (State == EB1LogState::Frozen)
	{
		if (Tension >= BreakawayTension)
		{
			SetState(EB1LogState::Free, Tension);
		}
		return;
	}

	const bool bAtRest = Tension < BreakawayTension
		&& Body->GetPhysicsLinearVelocity().Size() < RefreezeLinearSpeed
		&& Body->GetPhysicsAngularVelocityInDegrees().Size() < RefreezeAngularSpeed;
	RestTime = bAtRest ? RestTime + DeltaSeconds : 0.0f;
	if (RestTime >= RefreezeDelay)
	{
		SetState(EB1LogState::Frozen, Tension);
	}
}

void AB1WinchLog::SetState(EB1LogState NewState, float Tension)
{
	if (NewState == State)
	{
		return;
	}

	// Same physics proxy either way (kinematic flag); going free welds the root ball into the body.
	State = NewState;
	RestTime = 0.0f;
	++SwitchCount;
	Body->SetSimulatePhysics(State == EB1LogState::Free);
	if (State == EB1LogState::Free)
	{
		Body->WakeAllRigidBodies();
	}

	UE_LOG(LogB1Physics, Log, TEXT("%s log %s at tension %.0f N (breakaway %.0f), mass %.0f kg, yaw %+.1f deg, switch #%d"),
		*GetName(), State == EB1LogState::Free ? TEXT("free") : TEXT("frozen"), Tension, BreakawayTension, GetMass(),
		GetYawSincePlaced(), SwitchCount);
}

void AB1WinchLog::ResetToPlacedPose()
{
	SetState(EB1LogState::Frozen, GetTension());
	SetActorTransform(PlacedTransform, false, nullptr, ETeleportType::ResetPhysics);
	SwitchCount = 0;
}

float AB1WinchLog::GetTension() const
{
	const UB1WinchComponent* Winch = UB1WinchComponent::FindAttachedTo(Anchor);
	return Winch ? Winch->GetTension() : 0.0f;
}

float AB1WinchLog::GetYawSincePlaced() const
{
	return FMath::FindDeltaAngleDegrees(PlacedTransform.Rotator().Yaw, GetActorRotation().Yaw);
}

float AB1WinchLog::GetMass() const
{
	return Body->IsSimulatingPhysics() ? Body->GetMass() : TrunkMass + RootBallMass;
}

void AB1WinchLog::DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	const APlayerCameraManager* Camera = HUD && HUD->PlayerOwner ? HUD->PlayerOwner->PlayerCameraManager.Get() : nullptr;
	if (!Camera || GetWorld() != HUD->GetWorld()
		|| FVector::Dist(Camera->GetCameraLocation(), GetActorLocation()) > B1WinchShowDebug::AnchorDrawDistance)
	{
		return;
	}

	B1WinchShowDebug::DrawLog(Canvas, DisplayInfo, *this);
}
