#include "B1Character.h"
#include "B1GameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputActionValue.h"

AB1Character::AB1Character(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

UAbilitySystemComponent* AB1Character::GetAbilitySystemComponent() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPlayerState());
}

float AB1Character::GetCapsuleBottomZ() const
{
	return GetCapsuleComponent()->GetComponentLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void AB1Character::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Mapping contexts move to the controller's possession swap in S4-04.
	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (Subsystem && OnFootMappingContext)
	{
		Subsystem->AddMappingContext(OnFootMappingContext, OnFootMappingPriority);
		InputController = PC;
	}
}

void AB1Character::UnPossessed()
{
	RemoveOnFootMappingContext();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(B1GameplayTags::State_Avatar_OnFoot, 0);
	}

	Super::UnPossessed();
}

void AB1Character::RemoveOnFootMappingContext()
{
	const APlayerController* PC = InputController.Get();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (Subsystem && OnFootMappingContext)
	{
		Subsystem->RemoveMappingContext(OnFootMappingContext);
	}
	InputController.Reset();
}

void AB1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AB1Character::Move);
		EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &AB1Character::LookMouse);
		EnhancedInput->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AB1Character::LookStick);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
}

void AB1Character::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AB1Character::LookMouse(const FInputActionValue& Value)
{
	// Same sign as the template's Negate-Y modifier on Mouse2D, done here so the IMC needs none.
	const FVector2D Axis = Value.Get<FVector2D>() * MouseLookScale;
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AB1Character::LookStick(const FInputActionValue& Value)
{
	// Degrees per second straight into RotationInput, so the legacy input scales don't apply.
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || PC->IsLookInputIgnored())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	PC->RotationInput.Yaw += Axis.X * StickYawRate * DeltaSeconds;
	// Sign set by the S4-03 pad check: with += stick up looked down.
	PC->RotationInput.Pitch -= Axis.Y * StickPitchRate * DeltaSeconds;
}
