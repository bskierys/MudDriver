#include "B1VehicleShowDebug.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "DisplayDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace B1VehicleShowDebug
{
	const FName Category(TEXT("B1Vehicle"));

	static constexpr float TextHeightAboveActor = 250.0f;

	void Draw(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, const UChaosWheeledVehicleMovementComponent& Movement)
	{
		const AActor* Owner = Movement.GetOwner();
		if (!Canvas || !Owner || !DisplayInfo.IsDisplayOn(Category))
		{
			return;
		}

		const FVector ScreenPos = Canvas->Project(Owner->GetActorLocation() + FVector(0.0f, 0.0f, TextHeightAboveActor));
		if (ScreenPos.Z <= 0.0)
		{
			return;
		}

		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("%s"), *Owner->GetName()));
		Lines.Add(FString::Printf(TEXT("%.1f km/h  gear %d  %.0f rpm"),
			Movement.GetForwardSpeed() * 0.036f, Movement.GetCurrentGear(), Movement.GetEngineRotationSpeed()));

		for (int32 WheelIndex = 0; WheelIndex < Movement.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Movement.GetWheelState(WheelIndex);
			Lines.Add(FString::Printf(TEXT("w%d %-12s slip %5.2f%s %s"),
				WheelIndex,
				*GetNameSafe(Status.PhysMaterial.Get()),
				Status.SlipMagnitude,
				Status.bIsSlipping ? TEXT("!") : TEXT(" "),
				Status.bInContact ? TEXT("") : TEXT("(air)")));
		}

		const UFont* Font = GEngine->GetSmallFont();
		const float LineHeight = Font->GetMaxCharHeight();
		FFontRenderInfo RenderInfo;
		RenderInfo.bEnableShadow = true;

		float Y = ScreenPos.Y - LineHeight * Lines.Num();
		Canvas->SetDrawColor(FColor::Yellow);
		for (const FString& Line : Lines)
		{
			Canvas->DrawText(Font, Line, ScreenPos.X, Y, 1.0f, 1.0f, RenderInfo);
			Y += LineHeight;
		}
	}
}
