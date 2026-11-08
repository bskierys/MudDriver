#include "B1MudLensComponent.h"
#include "B1MudSprayComponent.h"
#include "B1MudSettings.h"
#include "B1MudLog.h"
#include "Camera/CameraComponent.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	TAutoConsoleVariable<bool> CVarMudLens(TEXT("b1.Mud.Lens"), true, TEXT("Mud splashed on the chase camera's lens"));

	/** Below this a slot counts as empty and the next splash gets a new placement */
	constexpr float EmptySlotStrength = 0.05f;
	constexpr float SplashIntervalJitter = 0.4f;
	constexpr float MinMaterialChange = 0.002f;
	constexpr int32 NumVariants = 8;

	const FName SlotParams[UB1MudLensComponent::NumSlots] = { TEXT("LensSlot0"), TEXT("LensSlot1"), TEXT("LensSlot2"), TEXT("LensSlot3") };
}

UB1MudLensComponent::UB1MudLensComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UB1MudLensComponent::BeginPlay()
{
	Super::BeginPlay();

	Random.Initialize(RandomSeed);
	for (FB1MudLensSlot& Slot : Slots)
	{
		PlaceSlot(Slot);
	}

	Spray = GetOwner()->FindComponentByClass<UB1MudSprayComponent>();
	if (Spray)
	{
		// Exposure reads this frame's launch data.
		AddTickPrerequisiteComponent(Spray);
	}
	else
	{
		UE_LOG(LogB1Mud, Warning, TEXT("%s: no UB1MudSprayComponent, the lens stays clean"), *GetNameSafe(GetOwner()));
	}

	FindCameras();
	AttachMaterial();
}

void UB1MudLensComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachMaterial();
	Super::EndPlay(EndPlayReason);
}

void UB1MudLensComponent::FindCameras()
{
	TArray<UCameraComponent*> Found;
	GetOwner()->GetComponents<UCameraComponent>(Found);
	Found.RemoveAll([this](const UCameraComponent* Camera) { return !Camera->ComponentHasTag(TargetTag); });
	Cameras = Found;

	FString Names;
	for (const UCameraComponent* Camera : Cameras)
	{
		Names += (Names.IsEmpty() ? TEXT("") : TEXT(",")) + Camera->GetName();
	}
	UE_LOG(LogB1Mud, Log, TEXT("%s lens cameras: %s"), *GetNameSafe(GetOwner()), Names.IsEmpty() ? TEXT("-") : *Names);
}

void UB1MudLensComponent::AttachMaterial()
{
	if (Cameras.IsEmpty())
	{
		return;
	}

	UMaterialInterface* Parent = GetDefault<UB1MudSettings>()->LensMaterial.LoadSynchronous();
	if (!Parent)
	{
		static bool bLogged = false;
		if (!bLogged)
		{
			bLogged = true;
			UE_LOG(LogB1Mud, Log, TEXT("UB1MudSettings::LensMaterial unset: lens mud is simulated but not shown"));
		}
		return;
	}

	Material = UMaterialInstanceDynamic::Create(Parent, this);
	for (UCameraComponent* Camera : Cameras)
	{
		// Appended, so blendables set on the camera by the host stay.
		Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Material));
	}
	bWrittenOnce = false;
	WriteMaterial();
}

void UB1MudLensComponent::DetachMaterial()
{
	if (!Material)
	{
		return;
	}

	for (UCameraComponent* Camera : Cameras)
	{
		if (Camera)
		{
			Camera->PostProcessSettings.WeightedBlendables.Array.RemoveAll([this](const FWeightedBlendable& Blendable) { return Blendable.Object == Material; });
		}
	}
	Material = nullptr;
}

float UB1MudLensComponent::GetLensDirt() const
{
	float Sum = 0.0f;
	for (const FB1MudLensSlot& Slot : Slots)
	{
		Sum += Slot.Strength;
	}
	return Sum / NumSlots;
}

float UB1MudLensComponent::GetShownLensDirt() const
{
	return PreviewLensDirt >= 0.0f ? PreviewLensDirt : GetLensDirt();
}

const FB1MudLensWheelExposure& UB1MudLensComponent::GetWheelExposure(int32 WheelIndex) const
{
	static const FB1MudLensWheelExposure None;
	return WheelExposures.IsValidIndex(WheelIndex) ? WheelExposures[WheelIndex] : None;
}

void UB1MudLensComponent::SetPreviewLensDirt(float Value)
{
	PreviewLensDirt = Value < 0.0f ? -1.0f : FMath::Min(Value, 1.0f);
	WriteMaterial();
}

void UB1MudLensComponent::SetLensDirt(float Value)
{
	const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
	for (FB1MudLensSlot& Slot : Slots)
	{
		Slot.Strength = Clamped;
		Slot.SinceHit = 0.0f;
	}
	LoggedTenth = FMath::FloorToInt32(Clamped * 10.0f + KINDA_SMALL_NUMBER);
	UE_LOG(LogB1Mud, Log, TEXT("%s lens set to %.2f"), *GetNameSafe(GetOwner()), Clamped);
	SetPreviewLensDirt(-1.0f);
}

void UB1MudLensComponent::Splash()
{
	AddSplash(LensSplashInterval * NumSlots / LensSecondsToFull);
	WriteMaterial();
}

void UB1MudLensComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FadeSlots(DeltaTime);
	UpdateExposure(GetActiveCamera());
	UpdateSplashes(DeltaTime);
	WriteMaterial();
	LogProgress();
}

const UCameraComponent* UB1MudLensComponent::GetActiveCamera() const
{
	for (const UCameraComponent* Camera : Cameras)
	{
		if (Camera && Camera->IsActive())
		{
			return Camera;
		}
	}
	return nullptr;
}

void UB1MudLensComponent::UpdateExposure(const UCameraComponent* Camera)
{
	Exposure = 0.0f;
	const int32 NumWheels = Spray ? Spray->GetNumWheels() : 0;
	WheelExposures.SetNum(NumWheels);
	const bool bEnabled = CVarMudLens.GetValueOnGameThread();

	for (int32 WheelIndex = 0; WheelIndex < NumWheels; ++WheelIndex)
	{
		FB1MudLensWheelExposure& Wheel = WheelExposures[WheelIndex];
		Wheel = FB1MudLensWheelExposure();
		const FVector Velocity = Spray->GetWheelSprayVelocity(WheelIndex);
		if (!Camera || !bEnabled || Velocity.IsNearlyZero())
		{
			continue;
		}

		const FVector ToCamera = Camera->GetComponentLocation() - Spray->GetWheelSprayOrigin(WheelIndex);
		Wheel.Intensity = Spray->GetWheelIntensity(WheelIndex);
		Wheel.LaunchSpeed = Velocity.Size();
		Wheel.Throw = FMath::SmoothStep(LensMinThrowSpeed, LensFullThrowSpeed, Wheel.LaunchSpeed);

		const FVector2D Throw2D = FVector2D(Velocity).GetSafeNormal();
		const FVector2D ToCamera2D = FVector2D(ToCamera).GetSafeNormal();
		Wheel.AimAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(Throw2D, ToCamera2D), -1.0f, 1.0f)));
		Wheel.Aim = 1.0f - FMath::SmoothStep(LensAimFull, FMath::Max(LensAimZero, LensAimFull + KINDA_SMALL_NUMBER), Wheel.AimAngle);

		Wheel.Distance = 1.0f / (1.0f + FMath::Square(ToCamera.Size() / LensFalloffDistance));
		Wheel.Exposure = Wheel.Intensity * Wheel.Throw * Wheel.Aim * Wheel.Distance;
		Exposure = FMath::Max(Exposure, Wheel.Exposure);
	}
}

void UB1MudLensComponent::UpdateSplashes(float DeltaTime)
{
	// The countdown keeps running below the threshold, so a flickering exposure can't splash faster than the interval.
	NextSplashIn = FMath::Max(NextSplashIn - DeltaTime, 0.0f);
	if (Exposure <= LensSplashThreshold || NextSplashIn > 0.0f)
	{
		return;
	}

	const float Interval = LensSplashInterval * Random.FRandRange(1.0f - SplashIntervalJitter, 1.0f + SplashIntervalJitter);
	NextSplashIn = Interval;
	// Mean of NumSlots slots, so LensDirt grows at Exposure / LensSecondsToFull.
	AddSplash(Interval * Exposure * NumSlots / LensSecondsToFull);
}

void UB1MudLensComponent::AddSplash(float Amount)
{
	int32 Emptiest = INDEX_NONE;
	int32 Weakest = 0;
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		if (Slots[Index].Strength < Slots[Weakest].Strength)
		{
			Weakest = Index;
		}
		if (Slots[Index].Strength < EmptySlotStrength && (Emptiest == INDEX_NONE || Slots[Index].Strength < Slots[Emptiest].Strength))
		{
			Emptiest = Index;
		}
	}

	FB1MudLensSlot& Slot = Slots[Emptiest != INDEX_NONE ? Emptiest : Weakest];
	if (Emptiest != INDEX_NONE)
	{
		PlaceSlot(Slot);
		Slot.Strength = 0.0f;
	}
	Slot.Strength = FMath::Min(Slot.Strength + Amount, 1.0f);
	Slot.SinceHit = 0.0f;
	++SplashCount;
}

void UB1MudLensComponent::FadeSlots(float DeltaTime)
{
	for (FB1MudLensSlot& Slot : Slots)
	{
		Slot.SinceHit += DeltaTime;
		if (Slot.SinceHit > LensHoldSeconds)
		{
			Slot.Strength = FMath::Max(Slot.Strength - DeltaTime / LensFadeSeconds, 0.0f);
		}
	}
}

void UB1MudLensComponent::PlaceSlot(FB1MudLensSlot& Slot)
{
	Slot.Offset = FVector2f(Random.FRand(), Random.FRand());
	Slot.Variant = Random.RandHelper(NumVariants);
}

void UB1MudLensComponent::WriteMaterial()
{
	if (!Material)
	{
		return;
	}

	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		const FB1MudLensSlot& Slot = Slots[Index];
		const float Strength = PreviewLensDirt >= 0.0f ? PreviewLensDirt : Slot.Strength;
		const FLinearColor Value(Strength, Slot.Offset.X, Slot.Offset.Y, static_cast<float>(Slot.Variant));
		const FLinearColor& Written = WrittenSlots[Index];
		// Exact zero is written, so a faded slot doesn't leave a few specks.
		const bool bChanged = !bWrittenOnce || Value.G != Written.G || Value.B != Written.B || Value.A != Written.A
			|| FMath::Abs(Value.R - Written.R) >= MinMaterialChange || (Value.R == 0.0f && Written.R != 0.0f);
		if (bChanged)
		{
			Material->SetVectorParameterValue(SlotParams[Index], Value);
			WrittenSlots[Index] = Value;
		}
	}
	bWrittenOnce = true;
}

void UB1MudLensComponent::LogProgress()
{
	const float Dirt = GetLensDirt();
	const int32 Tenth = FMath::FloorToInt32(Dirt * 10.0f + KINDA_SMALL_NUMBER);
	if (Tenth < LoggedTenth - 1)
	{
		// Faded a full tenth: rising again logs again. One tenth of slack, so fading between splashes at a boundary doesn't.
		LoggedTenth = Tenth + 1;
		return;
	}
	if (Tenth <= LoggedTenth)
	{
		return;
	}

	LoggedTenth = Tenth;
	UE_LOG(LogB1Mud, Log, TEXT("%s lens d=%.2f exp=%.2f splashes=%d"), *GetNameSafe(GetOwner()), Dirt, Exposure, SplashCount);
}
