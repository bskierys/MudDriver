#include "B1MudTrailSubsystem.h"
#include "B1MudPatch.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetRenderingLibrary.h"

namespace
{
	TAutoConsoleVariable<bool> CVarMudTrails(TEXT("b1.Mud.Trails"), true, TEXT("Wheel trails drawn into mud patches"));

	constexpr int32 BrushResolution = 64;

	/** Blend toward the stamp depth per stamp; neighbouring stamps overlap several times, so one pass reaches it.
	 *  A blend, not a max (canvas has no max mode): a shallower wheel partly fills an older rut */
	constexpr float StampOpacity = 0.5f;
}

void UB1MudTrailSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CreateBrushTexture();
}

bool UB1MudTrailSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UB1MudTrailSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UB1MudTrailSubsystem, STATGROUP_Tickables);
}

void UB1MudTrailSubsystem::CreateBrushTexture()
{
	BrushTexture = UTexture2D::CreateTransient(BrushResolution, BrushResolution, PF_B8G8R8A8);
	if (!BrushTexture)
	{
		return;
	}
	BrushTexture->SRGB = false;

	FTexture2DMipMap& Mip = BrushTexture->GetPlatformData()->Mips[0];
	FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	const float Center = (BrushResolution - 1) * 0.5f;
	for (int32 Y = 0; Y < BrushResolution; ++Y)
	{
		for (int32 X = 0; X < BrushResolution; ++X)
		{
			const float Distance = FVector2f(X - Center, Y - Center).Size() / Center;
			// White, so the draw colour sets the depth; the alpha falloff rounds the rut profile.
			const uint8 Alpha = FMath::RoundToInt(255.0f * (1.0f - FMath::SmoothStep(0.3f, 1.0f, Distance)));
			Pixels[Y * BrushResolution + X] = FColor(255, 255, 255, Alpha);
		}
	}
	Mip.BulkData.Unlock();
	BrushTexture->UpdateResource();
}

void UB1MudTrailSubsystem::RegisterPatch(AB1MudPatch* Patch)
{
	Patches.AddUnique(Patch);
}

void UB1MudTrailSubsystem::UnregisterPatch(AB1MudPatch* Patch)
{
	Patches.Remove(Patch);
	PendingStamps.Remove(Patch);
}

void UB1MudTrailSubsystem::AddSegment(const FVector& From, const FVector& To, float WidthCm, float DepthCm)
{
	if (!CVarMudTrails.GetValueOnGameThread())
	{
		return;
	}

	const int32 Steps = FMath::Max(1, FMath::FloorToInt(FVector::Dist2D(From, To) / StampSpacing));
	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		AddStamp(FMath::Lerp(From, To, static_cast<float>(Step) / Steps), WidthCm, DepthCm);
	}
}

void UB1MudTrailSubsystem::AddStamp(const FVector& WorldLocation, float WidthCm, float DepthCm)
{
	for (AB1MudPatch* Patch : Patches)
	{
		FVector2D UV;
		if (Patch && Patch->WorldToTrailUV(WorldLocation, UV))
		{
			PendingStamps.FindOrAdd(Patch).Add({ UV, WidthCm, DepthCm });
		}
	}
}

void UB1MudTrailSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (TPair<TWeakObjectPtr<AB1MudPatch>, TArray<FStamp>>& Pending : PendingStamps)
	{
		const AB1MudPatch* Patch = Pending.Key.Get();
		UTextureRenderTarget2D* Target = Patch ? Patch->GetTrailTarget() : nullptr;
		if (!Target || !BrushTexture || Pending.Value.IsEmpty())
		{
			continue;
		}

		UCanvas* Canvas = nullptr;
		FVector2D CanvasSize;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Target, Canvas, CanvasSize, Context);
		if (Canvas)
		{
			const FVector2D PixelsPerCm = CanvasSize / Patch->GetSize();
			for (const FStamp& Stamp : Pending.Value)
			{
				const FVector2D BrushSize = PixelsPerCm * Stamp.WidthCm;
				const float Depth = FMath::Clamp(Stamp.DepthCm / TrailDepthRange, 0.0f, 1.0f);
				Canvas->K2_DrawTexture(BrushTexture, Stamp.UV * CanvasSize - BrushSize * 0.5f, BrushSize, FVector2D::ZeroVector,
					FVector2D::UnitVector, FLinearColor(Depth, Depth, Depth, StampOpacity), BLEND_Translucent);
			}
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	}
	PendingStamps.Reset();
}
