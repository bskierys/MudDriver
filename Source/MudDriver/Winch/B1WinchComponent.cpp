#include "B1WinchComponent.h"
#include "B1WinchAnchorComponent.h"
#include "B1VehicleMovementComponent.h"
#include "B1Log.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CableComponent.h"
#include "GameFramework/Pawn.h"

UB1WinchComponent::UB1WinchComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UB1WinchComponent::BeginPlay()
{
	Super::BeginPlay();

	Movement = GetOwner()->FindComponentByClass<UB1VehicleMovementComponent>();
	if (!Movement)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("%s winch: no UB1VehicleMovementComponent, the cable won't pull"), *GetNameSafe(GetOwner()));
	}

	if (StartAnchor)
	{
		AttachTo(StartAnchor->FindComponentByClass<UB1WinchAnchorComponent>());
	}
}

void UB1WinchComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Detach();

	Super::EndPlay(EndPlayReason);
}

void UB1WinchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Anchor.IsValid() && Cable)
	{
		Detach();
	}

	ReadSimOutput();
	SendSimInput();
	SetWinchingTag(IsWinching());
}

void UB1WinchComponent::SendSimInput()
{
	if (!Movement)
	{
		return;
	}

	FB1WinchSimInput Input;
	if (const UB1WinchAnchorComponent* AnchorComponent = Anchor.Get())
	{
		const USceneComponent* Body = Movement->UpdatedComponent;
		Input.bAttached = true;
		Input.AnchorLocation = AnchorComponent->GetComponentLocation();
		Input.WinchPointLocal = Body ? GetComponentTransform().GetRelativeTransform(Body->GetComponentTransform()).GetLocation() : FVector::ZeroVector;
		Input.AttachSerial = AttachSerial;
		Input.InitialRestLength = InitialRestLength;
		Input.Pull = Pull;
		Input.MaxPullForce = MaxPullForce;
		Input.ReelInSpeed = ReelInSpeed;
		Input.MinRestLength = MinRestLength;
		Input.Stiffness = CableStiffness;
		Input.Damping = CableDamping;
	}
	Movement->SetWinchInput(Input);
}

void UB1WinchComponent::ReadSimOutput()
{
	if (!Movement || !IsAttached())
	{
		return;
	}

	// Until the sim has seen this attach, its output belongs to the previous cable.
	const FB1WinchSimOutput Output = Movement->GetWinchOutput();
	if (Output.AttachSerial != AttachSerial)
	{
		return;
	}

	Tension = Output.Tension;
	RestLength = Output.RestLength;
	PeakTension = FMath::Max(PeakTension, Tension);
	if (Cable)
	{
		Cable->CableLength = RestLength;
	}
}

void UB1WinchComponent::SetWinchingTag(bool bWinching)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());

	// The ASC is the driver's (ADR-004), so a parked jeep leaves it alone.
	const bool bWanted = bWinching && Pawn && Pawn->IsPawnControlled();
	if (!ASC || !WinchingTag.IsValid() || bWanted == bWinchingTagSet)
	{
		return;
	}

	ASC->SetLooseGameplayTagCount(WinchingTag, bWanted ? 1 : 0);
	bWinchingTagSet = bWanted;
}

bool UB1WinchComponent::AttachTo(UB1WinchAnchorComponent* NewAnchor)
{
	if (!NewAnchor)
	{
		return false;
	}

	const float Distance = FVector::Dist(GetComponentLocation(), NewAnchor->GetComponentLocation());
	if (Distance > MaxCableLength)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("%s winch: %s is %.1f m away, max %.1f m"), *GetNameSafe(GetOwner()),
			*GetNameSafe(NewAnchor->GetOwner()), Distance / 100.0f, MaxCableLength / 100.0f);
		return false;
	}

	Detach();

	Anchor = NewAnchor;
	++AttachSerial;
	InitialRestLength = Distance;
	RestLength = Distance;
	Tension = 0.0f;
	PeakTension = 0.0f;

	// A fresh cable starts its particles on the straight line between the ends.
	Cable = NewObject<UCableComponent>(GetOwner(), TEXT("WinchCable"));
	Cable->SetupAttachment(this);
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cable->SetAttachEndToComponent(NewAnchor);
	Cable->EndLocation = FVector::ZeroVector;
	Cable->CableLength = RestLength;
	Cable->NumSegments = CableSegments;
	Cable->CableWidth = CableWidth;
	// Default single iteration sags a 10+ m steel cable like a rope.
	Cable->SolverIterations = 16;
	Cable->bEnableStiffness = true;
	if (CableMaterial)
	{
		Cable->SetMaterial(0, CableMaterial);
	}
	Cable->RegisterComponent();

	SendSimInput();

	UE_LOG(LogB1Physics, Log, TEXT("%s winch attached to %s, rest %.2f m"), *GetNameSafe(GetOwner()),
		*GetNameSafe(NewAnchor->GetOwner()), RestLength / 100.0f);
	return true;
}

void UB1WinchComponent::Detach()
{
	// The cable lives exactly while attached; the anchor can already be gone at world teardown.
	if (Cable)
	{
		Cable->DestroyComponent();
		Cable = nullptr;

		UE_LOG(LogB1Physics, Log, TEXT("%s winch detached from %s: rest %.2f -> %.2f m, peak tension %.0f N (cap %.0f)"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Anchor.IsValid() ? Anchor->GetOwner() : nullptr), InitialRestLength / 100.0f,
			RestLength / 100.0f, PeakTension, MaxPullForce);
	}
	Anchor.Reset();
	RestLength = 0.0f;
	Tension = 0.0f;
	SetWinchingTag(false);
	SendSimInput();
}

void UB1WinchComponent::ToggleAttachNearest()
{
	if (IsAttached())
	{
		Detach();
		return;
	}

	if (UB1WinchAnchorComponent* Nearest = FindNearestAnchor())
	{
		AttachTo(Nearest);
	}
	else
	{
		UE_LOG(LogB1Physics, Log, TEXT("%s winch: no anchor within %.1f m"), *GetNameSafe(GetOwner()), MaxCableLength / 100.0f);
	}
}

UB1WinchAnchorComponent* UB1WinchComponent::FindNearestAnchor() const
{
	return UB1WinchAnchorComponent::FindNearest(GetWorld(), GetComponentLocation(), MaxCableLength);
}

float UB1WinchComponent::GetCurrentLength() const
{
	const UB1WinchAnchorComponent* AnchorComponent = Anchor.Get();
	return AnchorComponent ? FVector::Dist(GetComponentLocation(), AnchorComponent->GetComponentLocation()) : 0.0f;
}
