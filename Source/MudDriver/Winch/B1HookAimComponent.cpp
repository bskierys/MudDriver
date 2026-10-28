#include "B1HookAimComponent.h"
#include "B1InteractionScanComponent.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectIterator.h"

UB1HookAimComponent::UB1HookAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UB1HookAimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UB1WinchComponent* Winch = UB1WinchComponent::FindCarriedBy(GetOwner());
	if (!Winch)
	{
		HideGhost();
		return;
	}

	UB1WinchAnchorComponent* Target = nullptr;
	float Alpha = 0.0f;
	if (FindAim(*Winch, Target, Alpha))
	{
		Winch->SetAimTarget(Target, Alpha);
	}
	else
	{
		Winch->ClearAimTarget();
	}
	ShowGhost(*Winch, Target, Alpha);
}

bool UB1HookAimComponent::FindAim(const UB1WinchComponent& Winch, UB1WinchAnchorComponent*& OutAnchor, float& OutAlpha) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera)
	{
		return false;
	}

	const UB1InteractionScanComponent* Scan = Pawn->FindComponentByClass<UB1InteractionScanComponent>();
	const float AimRange = Scan ? Scan->Range : Range;
	const FVector Origin = Camera->GetComponentLocation();
	// Control rotation, not the camera's: the camera only picks it up when the view updates.
	const FVector Direction = Pawn->GetControlRotation().Vector();
	const float RayLength = AimRange * 2.0f;

	// A trunk the ray actually hits wins over one it only passes near.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(B1HookAim), false, Pawn);
	const AActor* HitActor = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * RayLength, ECC_Visibility, Params) ? Hit.GetActor() : nullptr;

	float BestScore = TNumericLimits<float>::Max();
	for (UB1WinchAnchorComponent* Anchor : TObjectRange<UB1WinchAnchorComponent>())
	{
		if (Anchor->GetWorld() != GetWorld() || !Anchor->IsRegistered()
			|| FVector::Dist(Pawn->GetActorLocation(), Anchor->GetClosestAreaPoint(Pawn->GetActorLocation())) > AimRange
			|| !Winch.IsInReach(Anchor))
		{
			continue;
		}

		// A thin trunk is easy to miss, so the closest point between ray and axis is the normal case.
		const float Alpha = Anchor->GetAreaAlphaClosestToRay(Origin, Direction, RayLength);
		const FVector Point = Anchor->GetAreaPoint(Alpha);
		const float Along = FMath::Max(0.0f, static_cast<float>((Point - Origin) | Direction));
		const float Score = Anchor->GetOwner() == HitActor ? -1.0f : FVector::Dist(Point, Origin + Direction * Along);
		if (Score < BestScore)
		{
			BestScore = Score;
			OutAnchor = Anchor;
			OutAlpha = Alpha;
		}
	}
	return OutAnchor != nullptr;
}

void UB1HookAimComponent::ShowGhost(const UB1WinchComponent& Winch, const UB1WinchAnchorComponent* Target, float Alpha)
{
	if (!Target)
	{
		HideGhost();
		return;
	}

	if (!Ghost)
	{
		Ghost = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("HookAimGhost"));
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		Ghost->SetUsingAbsoluteLocation(true);
		Ghost->SetUsingAbsoluteRotation(true);
		Ghost->SetUsingAbsoluteScale(true);
		Ghost->RegisterComponent();
	}
	// Same ring as the strap it turns into.
	Ghost->SetStaticMesh(Winch.StrapMesh);
	Ghost->SetMaterial(0, GhostMaterial);
	Ghost->SetWorldTransform(Target->GetRingTransform(Alpha, Winch.StrapClearance, Winch.StrapWidth));
	Ghost->SetVisibility(true);
}

void UB1HookAimComponent::HideGhost()
{
	if (Ghost)
	{
		Ghost->SetVisibility(false);
	}
}

void UB1HookAimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UB1WinchComponent* Winch = UB1WinchComponent::FindCarriedBy(GetOwner()))
	{
		Winch->ClearAimTarget();
	}
	if (Ghost)
	{
		Ghost->DestroyComponent();
		Ghost = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
