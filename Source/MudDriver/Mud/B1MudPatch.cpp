#include "B1MudPatch.h"
#include "B1MudSettings.h"
#include "B1MudShowDebug.h"
#include "B1MudTrailSurfaceComponent.h"
#include "ProceduralMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CubeMeshSize = 100.0f;
	constexpr float SurfaceThickness = 10.0f;
	/** Thick enough that a fast wheel or chassis can't tunnel through */
	constexpr float HardBottomThickness = 100.0f;

	/** Caps MudVisual at ~160k vertices on very large patches; the grid gets coarser instead */
	constexpr int32 MaxVisualCellsPerSide = 400;
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
	MudSurface->SetCollisionProfileName(GetDefault<UB1MudSettings>()->SurfaceProfileName);

	MudVisual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MudVisual"));
	MudVisual->SetupAttachment(Root);
	MudVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MudVisual->SetCastShadow(false);
	MudVisual->bUseAsyncCooking = false;

	TrailSurface = CreateDefaultSubobject<UB1MudTrailSurfaceComponent>(TEXT("TrailSurface"));

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
	MudSurface->SetCollisionProfileName(GetDefault<UB1MudSettings>()->SurfaceProfileName);
	MudSurface->SetVisibility(true);
	MudSurface->SetHiddenInGame(true);

	// The grid is built in BeginPlay: built here, its sections get saved into the actor package (13+ MB per patch).
	MudVisual->ClearAllMeshSections();
	MudVisual->SetMaterial(0, MudSurface->GetMaterial(0));

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

void AB1MudPatch::BeginPlay()
{
	Super::BeginPlay();

	ShowDebugHandle = AHUD::OnShowDebugInfo.AddUObject(this, &AB1MudPatch::DrawShowDebug);
	BuildVisualMesh();
	TrailSurface->Initialize(MudVisual, Size);
}

void AB1MudPatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AHUD::OnShowDebugInfo.Remove(ShowDebugHandle);

	Super::EndPlay(EndPlayReason);
}

void AB1MudPatch::DrawShowDebug(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	const APlayerCameraManager* Camera = HUD && HUD->PlayerOwner ? HUD->PlayerOwner->PlayerCameraManager.Get() : nullptr;
	if (!Camera || GetWorld() != HUD->GetWorld()
		|| FVector::Dist(Camera->GetCameraLocation(), GetActorLocation()) > B1MudShowDebug::PatchDrawDistance)
	{
		return;
	}

	B1MudShowDebug::DrawPatch(Canvas, DisplayInfo, *this);
}

void AB1MudPatch::BuildVisualMesh()
{
	const int32 CellsX = FMath::Clamp(FMath::CeilToInt(Size.X / VisualGridSpacing), 1, MaxVisualCellsPerSide);
	const int32 CellsY = FMath::Clamp(FMath::CeilToInt(Size.Y / VisualGridSpacing), 1, MaxVisualCellsPerSide);

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	const int32 NumVertices = (CellsX + 1) * (CellsY + 1);
	Vertices.Reserve(NumVertices);
	UVs.Reserve(NumVertices);
	for (int32 Y = 0; Y <= CellsY; ++Y)
	{
		for (int32 X = 0; X <= CellsX; ++X)
		{
			const FVector2D UV(static_cast<float>(X) / CellsX, static_cast<float>(Y) / CellsY);
			Vertices.Add(FVector((UV - FVector2D(0.5f)) * Size, 0.0f));
			UVs.Add(UV);
		}
	}
	Normals.Init(FVector::UpVector, NumVertices);
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), NumVertices);

	TArray<int32> Triangles;
	Triangles.Reserve(CellsX * CellsY * 6);
	for (int32 Y = 0; Y < CellsY; ++Y)
	{
		for (int32 X = 0; X < CellsX; ++X)
		{
			const int32 V00 = Y * (CellsX + 1) + X;
			const int32 V10 = V00 + 1;
			const int32 V01 = V00 + CellsX + 1;
			const int32 V11 = V01 + 1;
			Triangles.Append({ V00, V01, V10, V10, V01, V11 });
		}
	}

	MudVisual->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
}

FString AB1MudPatch::DescribeRamps() const
{
	TArray<FString> Edges;
	const bool bRampEnabled[] = { bRampPlusX, bRampMinusX, bRampPlusY, bRampMinusY };
	const TCHAR* Names[] = { TEXT("+X"), TEXT("-X"), TEXT("+Y"), TEXT("-Y") };
	for (int32 Edge = 0; Edge < UE_ARRAY_COUNT(Names); ++Edge)
	{
		if (bRampEnabled[Edge])
		{
			Edges.Add(Names[Edge]);
		}
	}
	return Edges.Num() > 0 ? FString::Join(Edges, TEXT(" ")) : FString(TEXT("none"));
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
