#include "B1WinchShowDebug.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "B1WinchLog.h"
#include "DisplayDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "UObject/UObjectIterator.h"

namespace B1WinchShowDebug
{
	const FName Category(TEXT("B1Winch"));
	const FName LogCategory(TEXT("B1Log"));

	static constexpr float WinchTextHeight = 100.0f;
	static constexpr float AnchorTextHeight = 250.0f;

	static void DrawLines(UCanvas* Canvas, const FVector& WorldAnchor, const TArray<FString>& Lines, const FColor& Color)
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

		float Y = ScreenPos.Y - LineHeight * Lines.Num();
		Canvas->SetDrawColor(Color);
		for (const FString& Line : Lines)
		{
			Canvas->DrawText(Font, Line, ScreenPos.X, Y, 1.0f, 1.0f, RenderInfo);
			Y += LineHeight;
		}
	}

	static void DrawAnchors(UCanvas* Canvas, const UB1WinchComponent& Winch)
	{
		const UWorld* World = Winch.GetWorld();
		const FVector WinchLocation = Winch.GetComponentLocation();
		for (TObjectIterator<UB1WinchAnchorComponent> It; It; ++It)
		{
			const UB1WinchAnchorComponent* Anchor = *It;
			if (Anchor->GetWorld() != World || !Anchor->GetOwner())
			{
				continue;
			}

			const float Distance = FVector::Dist(WinchLocation, Anchor->GetClosestAreaPoint(WinchLocation));
			if (Distance > AnchorDrawDistance)
			{
				continue;
			}

			const bool bInRange = Distance <= Winch.MaxCableLength;
			const bool bHooked = Winch.GetAnchor() == Anchor;
			// One frame each: ShowDebug redraws every frame.
			const FColor AreaColor = bHooked ? FColor::Orange : bInRange ? FColor::Green : FColor(160, 160, 160);
			DrawDebugLine(World, Anchor->GetAreaPoint(0.0f), Anchor->GetAreaPoint(1.0f), AreaColor, false, -1.0f, SDPG_Foreground, 3.0f);
			if (bHooked)
			{
				DrawDebugPoint(World, Winch.GetHookPoint(), 12.0f, FColor::Red, false, -1.0f, SDPG_Foreground);
			}
			TArray<FString> Lines;
			Lines.Add(Anchor->GetOwner()->GetActorNameOrLabel());
			Lines.Add(FString::Printf(TEXT("%.1f m  %s"), Distance / 100.0f,
				bHooked ? TEXT("hooked") : bInRange ? TEXT("in range") : TEXT("out of range")));
			DrawLines(Canvas, Anchor->GetComponentLocation() + FVector(0.0f, 0.0f, AnchorTextHeight), Lines,
				bInRange ? FColor::Green : FColor(160, 160, 160));
		}
	}

	void Draw(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const UB1WinchComponent& Winch, bool bDrawAnchors)
	{
		if (!Canvas || !DisplayInfo.IsDisplayOn(Category))
		{
			return;
		}

		TArray<FString> Lines;
		const UB1WinchAnchorComponent* Anchor = Winch.GetAnchor();
		if (Anchor && Anchor->GetOwner())
		{
			Lines.Add(FString::Printf(TEXT("winch  %s  alpha %.2f"), *Anchor->GetOwner()->GetActorNameOrLabel(), Winch.GetAnchorAlpha()));
			Lines.Add(FString::Printf(TEXT("tension %5.1f / %.1f kN"), Winch.GetTension() / 1000.0f, Winch.MaxPullForce / 1000.0f));
			Lines.Add(FString::Printf(TEXT("length %5.2f / %.0f m  rest %.2f m"), Winch.GetCurrentLength() / 100.0f,
				Winch.MaxCableLength / 100.0f, Winch.GetRestLength() / 100.0f));
			Lines.Add(FString::Printf(TEXT("hold %5.1f / %.1f kN  %s"), Winch.GetHoldForce() / 1000.0f,
				Winch.HandbrakeHoldForce / 1000.0f, Winch.IsHoldActive() ? TEXT("active") : TEXT("off")));
		}
		else if (const AActor* Carrier = Winch.GetHookCarrier())
		{
			Lines.Add(FString::Printf(TEXT("winch  hook carried by %s"), *Carrier->GetActorNameOrLabel()));
			Lines.Add(FString::Printf(TEXT("carry %5.2f / %.0f m"), Winch.GetCurrentLength() / 100.0f, Winch.MaxCableLength / 100.0f));
		}
		else
		{
			Lines.Add(FString::Printf(TEXT("winch  hook stowed  max %.0f m"), Winch.MaxCableLength / 100.0f));
		}
		Lines.Add(FString::Printf(TEXT("pull %+4.0f %%"), Winch.Pull * 100.0f));
		DrawLines(Canvas, Winch.GetComponentLocation() + FVector(0.0f, 0.0f, WinchTextHeight), Lines, FColor(255, 120, 120));

		if (bDrawAnchors)
		{
			DrawAnchors(Canvas, Winch);
		}
	}

	void DrawLog(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const AB1WinchLog& Log)
	{
		if (!Canvas || !DisplayInfo.IsDisplayOn(LogCategory))
		{
			return;
		}

		const bool bFree = Log.GetState() == EB1LogState::Free;
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("%s  %s"), *Log.GetActorNameOrLabel(), bFree ? TEXT("FREE") : TEXT("frozen")));
		Lines.Add(FString::Printf(TEXT("tension %5.1f / breakaway %.1f kN"), Log.GetTension() / 1000.0f, Log.BreakawayTension / 1000.0f));
		Lines.Add(FString::Printf(TEXT("switches %d  yaw %+.1f deg  mass %.0f kg"), Log.GetSwitchCount(), Log.GetYawSincePlaced(), Log.GetMass()));
		DrawLines(Canvas, Log.GetActorLocation() + FVector(0.0f, 0.0f, WinchTextHeight), Lines, bFree ? FColor::Yellow : FColor(140, 200, 255));
	}
}
