#include "B1WinchComponent.h"
#include "B1WinchAnchorComponent.h"
#include "B1VehicleMovementComponent.h"
#include "B1Log.h"
#include "B1MudSettings.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

UB1WinchComponent::UB1WinchComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	StrapMesh = Cylinder.Object;
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
		AttachTo(StartAnchor->FindComponentByClass<UB1WinchAnchorComponent>(), StartAnchorAlpha);
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
	UpdateAttachedCableEnd();
	ReadSimOutput();
	SendSimInput();
	UpdateOwnerTags();
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
		Input.AnchorLocation = GetHookPoint();
		if (const UPrimitiveComponent* AnchorBody = AnchorComponent->GetSimulatingBody())
		{
			Input.AnchorProxy = AnchorBody->GetBodyInstance()->GetPhysicsActor();
			// The physics body frame has no scale, so no GetRelativeTransform (a scaled log would put the point off).
			Input.AnchorPointLocal = AnchorBody->GetComponentQuat().UnrotateVector(Input.AnchorLocation - AnchorBody->GetComponentLocation());
		}
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
		Input.HandbrakeHoldForce = HandbrakeHoldForce;
		Input.HandbrakeHoldSpeed = HandbrakeHoldSpeed;
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
	HoldForce = Output.HoldForce;
	bHoldActive = Output.bHoldActive;
	PeakTension = FMath::Max(PeakTension, Tension);
	if (Cable)
	{
		Cable->CableLength = RestLength;
		// The verlet chain can't converge against gravity over this many segments, so a loaded cable drops it instead.
		Cable->CableGravityScale = 1.0f - FMath::Clamp(Tension / CableTautTension, 0.0f, 1.0f);
	}
}

void UB1WinchComponent::UpdateOwnerTags()
{
	// The ASC is the driver's (ADR-004), so a parked jeep leaves it alone.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const bool bControlled = Pawn && Pawn->IsPawnControlled();
	SetOwnerTag(WinchingTag, bControlled && IsWinching(), bWinchingTagSet);
	SetOwnerTag(HookedTag, bControlled && IsAttached(), bHookedTagSet);
}

void UB1WinchComponent::SetOwnerTag(const FGameplayTag& Tag, bool bWanted, bool& bTagSet)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC || !Tag.IsValid() || bWanted == bTagSet)
	{
		return;
	}

	ASC->SetLooseGameplayTagCount(Tag, bWanted ? 1 : 0);
	bTagSet = bWanted;
}

bool UB1WinchComponent::AttachTo(UB1WinchAnchorComponent* NewAnchor, float AreaAlpha)
{
	if (!NewAnchor)
	{
		return false;
	}

	const float Alpha = AreaAlpha < 0.0f ? NewAnchor->GetAreaAlphaClosestTo(GetComponentLocation()) : FMath::Min(AreaAlpha, 1.0f);
	const float Distance = FVector::Dist(GetComponentLocation(), NewAnchor->GetAreaPoint(Alpha));
	if (Distance > MaxCableLength)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("%s winch: %s is %.1f m away, max %.1f m"), *GetNameSafe(GetOwner()),
			*GetNameSafe(NewAnchor->GetOwner()), Distance / 100.0f, MaxCableLength / 100.0f);
		return false;
	}

	ReleaseAnchor();
	HookCarrier.Reset();

	Anchor = NewAnchor;
	AnchorAlpha = Alpha;
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
	UpdateAttachedCableEnd();
	CreateStrap(*NewAnchor);

	SendSimInput();

	UE_LOG(LogB1Physics, Log, TEXT("%s winch attached to %s at alpha %.2f, rest %.2f m"), *GetNameSafe(GetOwner()),
		*GetNameSafe(NewAnchor->GetOwner()), AnchorAlpha, RestLength / 100.0f);
	return true;
}

void UB1WinchComponent::SetAimTarget(UB1WinchAnchorComponent* Target, float Alpha)
{
	AimAnchor = Target;
	AimAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
}

FVector UB1WinchComponent::GetHookPoint() const
{
	return Anchor.IsValid() ? Anchor->GetAreaPoint(AnchorAlpha) : GetComponentLocation();
}

void UB1WinchComponent::UpdateAttachedCableEnd()
{
	const UB1WinchAnchorComponent* AnchorComponent = Anchor.Get();
	if (!Cable || !AnchorComponent)
	{
		return;
	}

	// EndLocation is in the end component's space, scale included.
	const FVector Surface = AnchorComponent->GetSurfacePoint(AnchorAlpha, GetComponentLocation(), StrapClearance);
	Cable->EndLocation = AnchorComponent->GetComponentTransform().InverseTransformPosition(Surface);
}

void UB1WinchComponent::CreateStrap(UB1WinchAnchorComponent& Target)
{
	if (!StrapMesh)
	{
		return;
	}

	Strap = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("WinchStrap"));
	Strap->SetStaticMesh(StrapMesh);
	Strap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Strap->SetCastShadow(false);
	if (StrapMaterial)
	{
		Strap->SetMaterial(0, StrapMaterial);
	}
	Strap->RegisterComponent();

	Strap->SetWorldTransform(Target.GetRingTransform(AnchorAlpha, StrapClearance, StrapWidth));
	// Follows a moving anchor (log); KeepWorld so a scaled parent doesn't distort it.
	Strap->AttachToComponent(&Target, FAttachmentTransformRules::KeepWorldTransform);
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
		Cable->CableGravityScale = 1.0f;
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
	// Rests on a log lying under it.
	CableComponent.SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
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
	ClearAimTarget();
	if (Strap)
	{
		Strap->DestroyComponent();
		Strap = nullptr;
	}
	if (Cable)
	{
		Cable->EndLocation = FVector::ZeroVector;
	}
	RestLength = 0.0f;
	Tension = 0.0f;
	HoldForce = 0.0f;
	bHoldActive = false;
	UpdateOwnerTags();
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
	return Target && FVector::Dist(GetComponentLocation(), Target->GetClosestAreaPoint(GetComponentLocation())) <= MaxCableLength;
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
	if (Anchor.IsValid())
	{
		return FVector::Dist(GetComponentLocation(), GetHookPoint());
	}
	return HookCarrier.IsValid() ? FVector::Dist(GetComponentLocation(), HookCarrier->GetComponentLocation()) : 0.0f;
}
