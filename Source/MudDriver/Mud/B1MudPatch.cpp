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
}
