#include "B1MudTrailSurfaceComponent.h"
#include "B1MudTrailSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FName TrailMaskParam(TEXT("TrailMask"));
}

void UB1MudTrailSurfaceComponent::Initialize(UPrimitiveComponent* InSurface, const FVector2D& InSize)
{
	UB1MudTrailSubsystem* Trails = GetWorld()->GetSubsystem<UB1MudTrailSubsystem>();
	if (!Trails || !InSurface)
	{
		return;
	}

	Surface = InSurface;
	Size = InSize;

	// Contacts sit below the surface by the sink depth, so the box reaches down past the deepest rut.
	const FTransform& Transform = Surface->GetComponentTransform();
	const FVector HalfSize(Size * 0.5f, 0.0f);
	WorldBounds = FBox(-HalfSize, HalfSize).TransformBy(Transform).ExpandBy(FVector(0.0f, 0.0f, UB1MudTrailSubsystem::TrailDepthRange));

	// Half float: 8-bit depth steps (0.4 cm) show up as stripes once the material takes the normal from the gradient.
	TrailTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Resolution, Resolution, RTF_R16f, FLinearColor::Black);
	if (UMaterialInstanceDynamic* Material = Surface->CreateDynamicMaterialInstance(0))
	{
		Material->SetTextureParameterValue(TrailMaskParam, TrailTarget);
		Material->SetScalarParameterValue(TEXT("TrailDepthRange"), UB1MudTrailSubsystem::TrailDepthRange);
		Material->SetScalarParameterValue(TEXT("PatchSizeX"), Size.X);
		Material->SetScalarParameterValue(TEXT("PatchSizeY"), Size.Y);
	}

	Trails->RegisterSurface(this);
}

void UB1MudTrailSurfaceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UB1MudTrailSubsystem* Trails = GetWorld()->GetSubsystem<UB1MudTrailSubsystem>())
	{
		Trails->UnregisterSurface(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool UB1MudTrailSurfaceComponent::WorldToTrailUV(const FVector& WorldLocation, FVector2D& OutUV) const
{
	if (!Surface)
	{
		return false;
	}

	const FVector Local = Surface->GetComponentTransform().InverseTransformPosition(WorldLocation);
	OutUV = FVector2D(Local.X, Local.Y) / Size + FVector2D(0.5f);
	return OutUV.X >= 0.0f && OutUV.X <= 1.0f && OutUV.Y >= 0.0f && OutUV.Y <= 1.0f;
}
