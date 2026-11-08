#include "B1MudShowDebug.h"
#include "B1MudVehicleComponent.h"
#include "B1MudDirtComponent.h"
#include "B1PhysicalMaterial_Mud.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "B1MudPatch.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DisplayDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace B1MudShowDebug
{
	const FName Category(TEXT("B1Mud"));

	static constexpr float VehicleTextHeight = 250.0f;
	static constexpr float PatchTextHeight = 150.0f;

	static void DrawLines(UCanvas* Canvas, const FVector& WorldAnchor, const TArray<FString>& Lines, const FColor& Color, bool bBelowAnchor)
	{
		const FVector ScreenPos = Canvas->Project(WorldAnchor);
		if (ScreenPos.Z <= 0.0)
		{
			return;
		}

		const UFont* Font = GEngine->GetSmallFont();
		const float LineHeight = Font->GetMaxCharHeight();
		FFontRenderInfo RenderInfo;
		RenderInfo.bEnableShadow = true;

		float Y = bBelowAnchor ? ScreenPos.Y : ScreenPos.Y - LineHeight * Lines.Num();
		Canvas->SetDrawColor(Color);
		for (const FString& Line : Lines)
		{
			Canvas->DrawText(Font, Line, ScreenPos.X, Y, 1.0f, 1.0f, RenderInfo);
			Y += LineHeight;
		}
	}

	void DrawVehicle(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const UB1MudVehicleComponent& Mud)
	{
		const AActor* Owner = Mud.GetOwner();
		if (!Canvas || !Owner || !DisplayInfo.IsDisplayOn(Category))
		{
			return;
		}

		TArray<FString> Lines;
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
		const TCHAR* InMud = !AbilitySystem ? TEXT("n/a (no ASC)")
			: AbilitySystem->HasMatchingGameplayTag(Mud.InMudTag) ? TEXT("yes") : TEXT("no");
		Lines.Add(FString::Printf(TEXT("mud  InMud %s  belly %.1f cm / %.0f N"), InMud, Mud.GetBellyDepth(), Mud.GetBellyDrag()));

		if (const UB1MudDirtComponent* Dirt = Owner->FindComponentByClass<UB1MudDirtComponent>())
		{
			Lines.Add(FString::Printf(TEXT("dirt w %.2f/%.2f/%.2f/%.2f  body %.2f%s"), Dirt->GetWheelDirt(0), Dirt->GetWheelDirt(1),
				Dirt->GetWheelDirt(2), Dirt->GetWheelDirt(3), Dirt->GetBodyDirt(),
				Dirt->PreviewDirt >= 0.0f ? *FString::Printf(TEXT("  (preview %.2f)"), Dirt->PreviewDirt) : TEXT("")));
		}

		for (int32 WheelIndex = 0; WheelIndex < Mud.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Mud.GetWheelState(WheelIndex);
			const UB1PhysicalMaterial_Mud* WheelMud = Mud.GetWheelMud(WheelIndex);
			Lines.Add(FString::Printf(TEXT("w%d %-18s sink %5.1f / %4.0f cm  mu %.2f  rr %5.0f N%s"),
				WheelIndex,
				*GetNameSafe(Status.PhysMaterial.Get()),
				Mud.GetWheelSinkDepth(WheelIndex),
				WheelMud ? WheelMud->MaxSinkDepth : 0.0f,
				Mud.GetWheelMudFrictionScale(WheelIndex),
				Mud.GetWheelRollingResistance(WheelIndex),
				Status.bInContact ? TEXT("") : TEXT(" (air)")));
		}

		DrawLines(Canvas, Owner->GetActorLocation() + FVector(0.0f, 0.0f, VehicleTextHeight), Lines, FColor::Orange, true);
	}

	void DrawPatch(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const AB1MudPatch& Patch)
	{
		if (!Canvas || !DisplayInfo.IsDisplayOn(Category))
		{
			return;
		}

		const UPhysicalMaterial* Material = Patch.GetMudMaterial();
		const UB1PhysicalMaterial_Mud* Mud = Cast<UB1PhysicalMaterial_Mud>(Material);
		const FVector2D Size = Patch.GetSize() / 100.0f;

		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("%s  %s"), *Patch.GetActorNameOrLabel(), *GetNameSafe(Material)));
		Lines.Add(Mud ? FString::Printf(TEXT("max sink %.0f cm  static %.0f%%  %.0f x %.0f m"), Mud->MaxSinkDepth, Mud->StaticSinkRatio * 100.0f, Size.X, Size.Y)
			: FString::Printf(TEXT("not B1 mud  %.0f x %.0f m"), Size.X, Size.Y));
		Lines.Add(Patch.HasHardBottom()
			? FString::Printf(TEXT("bottom %.0f cm  ramps %s (%.0f cm)"), Patch.GetHardBottomDepth(), *Patch.DescribeRamps(), Patch.GetEdgeRampLength())
			: FString(TEXT("no hard bottom")));

		DrawLines(Canvas, Patch.GetActorLocation() + FVector(0.0f, 0.0f, PatchTextHeight), Lines, FColor::Cyan, false);
	}
}
