#include "B1MudPatch.h"
#include "B1CollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CubeMeshSize = 100.0f;
	constexpr float SurfaceThickness = 10.0f;
	/** Thick enough that a fast wheel or chassis can't tunnel through */
	constexpr float HardBottomThickness = 100.0f;
}

AB1MudPatch::AB1MudPatch()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	MudSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MudSurface"));
	MudSurface->SetupAttachment(Root);
	MudSurface->SetMobility(EComponentMobility::Static);
	MudSurface->SetStaticMesh(CubeMesh.Object);
	MudSurface->SetCollisionProfileName(B1_CollisionProfile_MudSurface);

	HardBottom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HardBottom"));
	HardBottom->SetupAttachment(Root);
	HardBottom->SetMobility(EComponentMobility::Static);
	HardBottom->SetStaticMesh(CubeMesh.Object);
	HardBottom->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	for (const TCHAR* Name : { TEXT("EdgeRampPlusX"), TEXT("EdgeRampMinusX"), TEXT("EdgeRampPlusY"), TEXT("EdgeRampMinusY") })
	{
		UStaticMeshComponent* Ramp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Ramp->SetupAttachment(Root);
		Ramp->SetMobility(EComponentMobility::Static);
		Ramp->SetStaticMesh(CubeMesh.Object);
		Ramp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		EdgeRamps.Add(Ramp);
	}
}

void AB1MudPatch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	MudSurface->SetRelativeLocation(FVector(0.0f, 0.0f, -SurfaceThickness * 0.5f));
	MudSurface->SetRelativeScale3D(FVector(Size.X, Size.Y, SurfaceThickness) / CubeMeshSize);
	MudSurface->SetPhysMaterialOverride(MudMaterial);

	HardBottom->SetRelativeLocation(FVector(0.0f, 0.0f, -HardBottomDepth - HardBottomThickness * 0.5f));
	HardBottom->SetRelativeScale3D(FVector(Size.X, Size.Y, HardBottomThickness) / CubeMeshSize);
	HardBottom->SetVisibility(bHardBottom);
	HardBottom->SetCollisionProfileName(bHardBottom ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);

	const bool bRampEnabled[] = { bRampPlusX, bRampMinusX, bRampPlusY, bRampMinusY };
	const FVector2D Outward[] = { FVector2D(1.0f, 0.0f), FVector2D(-1.0f, 0.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, -1.0f) };
	for (int32 Edge = 0; Edge < EdgeRamps.Num(); ++Edge)
	{
		UpdateEdgeRamp(*EdgeRamps[Edge], Outward[Edge], bRampEnabled[Edge]);
	}
}

void AB1MudPatch::UpdateEdgeRamp(UStaticMeshComponent& Ramp, const FVector2D& Outward, bool bEnabled)
{
	const bool bActive = bEnabled && bHardBottom && HardBottomDepth > 0.0f;
	Ramp.SetVisibility(bActive);
	Ramp.SetCollisionProfileName(bActive ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	if (!bActive)
	{
		return;
	}

	const float HalfExtent = FMath::Abs(Outward.X) * Size.X * 0.5f + FMath::Abs(Outward.Y) * Size.Y * 0.5f;
	const float EdgeWidth = FMath::Abs(Outward.X) * Size.Y + FMath::Abs(Outward.Y) * Size.X;
	const float Length = FMath::Min(EdgeRampLength, HalfExtent * 2.0f);
	const float SlopeLength = FMath::Sqrt(Length * Length + HardBottomDepth * HardBottomDepth);

	// A box pitched so its top face runs from the hard bottom up to the surface at the edge.
	const FRotator Rotation(FMath::RadiansToDegrees(FMath::Atan2(HardBottomDepth, Length)), FMath::RadiansToDegrees(FMath::Atan2(Outward.Y, Outward.X)), 0.0f);
	const FVector TopCenter = FVector(Outward * (HalfExtent - Length * 0.5f), -HardBottomDepth * 0.5f);
	const FVector Location = TopCenter - Rotation.RotateVector(FVector::UpVector) * HardBottomThickness * 0.5f;

	Ramp.SetRelativeLocationAndRotation(Location, Rotation);
	Ramp.SetRelativeScale3D(FVector(SlopeLength, EdgeWidth, HardBottomThickness) / CubeMeshSize);
	Ramp.SetMaterial(0, HardBottom->GetMaterial(0));
}
