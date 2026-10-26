#include "B1MudShowDebug.h"
#include "B1VehicleMovementComponent.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1MudPatch.h"
#include "B1GameplayTags.h"
#include "AbilitySystemComponent.h"
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

	void DrawVehicle(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const UB1VehicleMovementComponent& Movement, const UAbilitySystemComponent* AbilitySystem)
	{
		const AActor* Owner = Movement.GetOwner();
		if (!Canvas || !Owner || !DisplayInfo.IsDisplayOn(Category))
		{
			return;
		}

		TArray<FString> Lines;
		const TCHAR* InMud = !AbilitySystem ? TEXT("n/a (no ASC)")
			: AbilitySystem->HasMatchingGameplayTag(B1GameplayTags::Vehicle_State_InMud) ? TEXT("yes") : TEXT("no");
		Lines.Add(FString::Printf(TEXT("mud  InMud %s  belly %.1f cm / %.0f N"), InMud, Movement.GetBellyDepth(), Movement.GetBellyDrag()));

		for (int32 WheelIndex = 0; WheelIndex < Movement.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Movement.GetWheelState(WheelIndex);
			const UB1PhysicalMaterial_Mud* Mud = Cast<UB1PhysicalMaterial_Mud>(Status.PhysMaterial.Get());
			Lines.Add(FString::Printf(TEXT("w%d %-18s sink %5.1f / %4.0f cm  mu %.2f  rr %5.0f N%s"),
				WheelIndex,
				*GetNameSafe(Status.PhysMaterial.Get()),
				Movement.GetWheelSinkDepth(WheelIndex),
				Mud ? Mud->MaxSinkDepth : 0.0f,
				Movement.GetWheelMudFrictionScale(WheelIndex),
				Movement.GetWheelRollingResistance(WheelIndex),
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
