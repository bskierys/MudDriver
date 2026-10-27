#include "B1WinchComponent.h"
#include "B1WinchAnchorComponent.h"
#include "B1VehicleMovementComponent.h"
#include "B1Log.h"
#include "B1MudSettings.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CableComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UObjectIterator.h"

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

	if (Cable && !Anchor.IsValid() && !HookCarrier.IsValid())
	{
		Detach();
	}

	TickCarriedHook();
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
		Input.ReelOutSpeed = ReelOutSpeed;
		Input.MinRestLength = MinRestLength;
		Input.MaxCableLength = MaxCableLength;
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
	if (!IsInReach(NewAnchor))
	{
		UE_LOG(LogB1Physics, Warning, TEXT("%s winch: %s is %.1f m away, max %.1f m"), *GetNameSafe(GetOwner()),
			*GetNameSafe(NewAnchor->GetOwner()), Distance / 100.0f, MaxCableLength / 100.0f);
		return false;
	}

	ReleaseAnchor();
	HookCarrier.Reset();

	Anchor = NewAnchor;
	++AttachSerial;
	InitialRestLength = Distance;
	RestLength = Distance;
	Tension = 0.0f;
	PeakTension = 0.0f;

	// A carried hook keeps the cable lying where it is.
	if (Cable)
	{
		Cable->SetAttachEndToComponent(NewAnchor);
		Cable->CableLength = RestLength;
	}
	else
	{
		CreateCable(*NewAnchor, RestLength);
	}

	SendSimInput();

	UE_LOG(LogB1Physics, Log, TEXT("%s winch attached to %s, rest %.2f m"), *GetNameSafe(GetOwner()),
		*GetNameSafe(NewAnchor->GetOwner()), RestLength / 100.0f);
	return true;
}

void UB1WinchComponent::CreateCable(USceneComponent& End, float Length)
{
	// A fresh cable starts its particles on the straight line between the ends.
	Cable = NewObject<UCableComponent>(GetOwner(), TEXT("WinchCable"));
	Cable->SetupAttachment(this);
	SetupCableCollision(*Cable);
	Cable->SetAttachEndToComponent(&End);
	Cable->EndLocation = FVector::ZeroVector;
	Cable->CableLength = Length;
	Cable->NumSegments = FMath::Clamp(FMath::CeilToInt(MaxCableLength / CableSegmentLength), 4, 400);
	Cable->CableWidth = CableWidth;
	// Default single iteration sags a 10+ m steel cable like a rope.
	Cable->SolverIterations = CableSolverIterations;
	Cable->bEnableStiffness = true;
	if (CableMaterial)
	{
		Cable->SetMaterial(0, CableMaterial);
	}
	Cable->RegisterComponent();
}

bool UB1WinchComponent::TakeHook(USceneComponent* Hand)
{
	if (!Hand || GetHookState() != EB1WinchHook::Stowed)
	{
		return false;
	}

	CarryTo(*Hand, 0.0f);
	return true;
}

bool UB1WinchComponent::DetachToCarrier(USceneComponent* Hand)
{
	if (!Hand || !IsAttached())
	{
		return false;
	}

	const float Length = RestLength;
	ReleaseAnchor();
	CarryTo(*Hand, Length);
	return true;
}

void UB1WinchComponent::CarryTo(USceneComponent& Hand, float Length)
{
	HookCarrier = &Hand;
	CarryLength = FMath::Min(FMath::Max(Length, GetCurrentLength()), MaxCableLength);
	if (Cable)
	{
		Cable->SetAttachEndToComponent(&Hand);
		Cable->CableLength = CarryLength;
	}
	else
	{
		CreateCable(Hand, CarryLength);
	}

	UE_LOG(LogB1Physics, Log, TEXT("%s winch hook carried by %s, %.2f m out"), *GetNameSafe(GetOwner()),
		*GetNameSafe(Hand.GetOwner()), CarryLength / 100.0f);
}

void UB1WinchComponent::TickCarriedHook()
{
	const USceneComponent* Hand = HookCarrier.Get();
	if (!Hand)
	{
		return;
	}

	FVector Offset = Hand->GetComponentLocation() - GetComponentLocation();
	float Distance = Offset.Size();
	ACharacter* Carrier = Cast<ACharacter>(Hand->GetOwner());
	if (Carrier && Distance > MaxCableLength)
	{
		const FVector Outward = Offset / Distance;
		const FVector Leash = -Outward * (Distance - MaxCableLength);
		UCharacterMovementComponent* CharacterMovement = Carrier->GetCharacterMovement();
		FHitResult Hit;
		CharacterMovement->SafeMoveUpdatedComponent(Leash, Carrier->GetActorQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			// Public on the base class, protected on the character's.
			static_cast<UMovementComponent*>(CharacterMovement)->SlideAlongSurface(Leash, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
		CharacterMovement->Velocity -= FMath::Max(0.0, CharacterMovement->Velocity | Outward) * Outward;
		Distance = FVector::Dist(Hand->GetComponentLocation(), GetComponentLocation());
	}

	// Paid out as the carrier walks away; walking back leaves the slack on the ground.
	CarryLength = FMath::Min(FMath::Max(CarryLength, Distance), MaxCableLength);
	if (Cable)
	{
		Cable->CableLength = CarryLength;
	}
}

void UB1WinchComponent::SetupCableCollision(UCableComponent& CableComponent) const
{
	// The cable sweeps with its own object type and responses and can't ignore its owner, so filter by type.
	// Mud surfaces block only WorldDynamic, hence that object type.
	CableComponent.bEnableCollision = bCableCollision;
	CableComponent.CollisionFriction = CableCollisionFriction;
	CableComponent.SetCollisionEnabled(bCableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	CableComponent.SetCollisionObjectType(ECC_WorldDynamic);
	CableComponent.SetCollisionResponseToAllChannels(ECR_Ignore);
	CableComponent.SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CableComponent.SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CableComponent.SetCollisionResponseToChannel(GetDefault<UB1MudSettings>()->MudObjectChannel, ECR_Block);
}

void UB1WinchComponent::ReleaseAnchor()
{
	// Not explicitly null = it was hooked; the anchor itself can already be gone at world teardown.
	if (!Anchor.IsExplicitlyNull())
	{
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

void UB1WinchComponent::Detach()
{
	ReleaseAnchor();
	if (!HookCarrier.IsExplicitlyNull())
	{
		UE_LOG(LogB1Physics, Log, TEXT("%s winch hook stowed"), *GetNameSafe(GetOwner()));
		HookCarrier.Reset();
	}
	if (Cable)
	{
		Cable->DestroyComponent();
		Cable = nullptr;
	}
}

EB1WinchHook UB1WinchComponent::GetHookState() const
{
	if (Anchor.IsValid())
	{
		return EB1WinchHook::Attached;
	}
	return HookCarrier.IsValid() ? EB1WinchHook::Carried : EB1WinchHook::Stowed;
}

bool UB1WinchComponent::IsInReach(const UB1WinchAnchorComponent* Target) const
{
	return Target && FVector::Dist(GetComponentLocation(), Target->GetComponentLocation()) <= MaxCableLength;
}

UB1WinchComponent* UB1WinchComponent::FindCarriedBy(const AActor* Carrier)
{
	for (UB1WinchComponent* Winch : TObjectRange<UB1WinchComponent>())
	{
		if (Carrier && Winch->GetWorld() == Carrier->GetWorld() && Winch->GetHookCarrier() == Carrier)
		{
			return Winch;
		}
	}
	return nullptr;
}

UB1WinchComponent* UB1WinchComponent::FindAttachedTo(const UB1WinchAnchorComponent* Target)
{
	for (UB1WinchComponent* Winch : TObjectRange<UB1WinchComponent>())
	{
		if (Target && Winch->GetWorld() == Target->GetWorld() && Winch->GetAnchor() == Target)
		{
			return Winch;
		}
	}
	return nullptr;
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
	const USceneComponent* HookEnd = Anchor.IsValid() ? static_cast<const USceneComponent*>(Anchor.Get()) : HookCarrier.Get();
	return HookEnd ? FVector::Dist(GetComponentLocation(), HookEnd->GetComponentLocation()) : 0.0f;
}
