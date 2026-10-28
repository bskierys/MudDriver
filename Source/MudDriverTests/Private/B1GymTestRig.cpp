#include "B1GymTestRig.h"
#include "B1Log.h"
#include "B1AbilitySystemComponent.h"
#include "B1Character.h"
#include "B1Jeep.h"
#include "Components/CapsuleComponent.h"
#include "B1GameplayTags.h"
#include "B1PlayerController.h"
#include "B1VehicleAttributeSet.h"
#include "B1VehicleMovementComponent.h"
#include "B1MudVehicleComponent.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "AbilitySystemGlobals.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreMisc.h"
#include "TimerManager.h"
#include "WheeledVehiclePawn.h"

namespace
{
	constexpr float CmPerM = 100.0f;
	/** Spare time on top of the script before the framework times the test out */
	constexpr float TimeLimitMargin = 10.0f;
}

AB1GymTestRig::AB1GymTestRig()
{
	InputScript.Add({ 5.0f, 1.0f, 0.0f, 0.0f, false });
}

void AB1GymTestRig::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRun && !GIsAutomationTesting)
	{
		GetWorldTimerManager().SetTimerForNextTick([this]()
		{
			if (!IsRunning())
			{
				RunTest();
			}
		});
	}
}

void AB1GymTestRig::PrepareTest()
{
	Super::PrepareTest();

	bScriptRunning = false;
	ScriptDuration = 0.0f;
	for (const FB1GymInputStep& Step : InputScript)
	{
		ScriptDuration += Step.Duration;
	}

	PreparationTimeLimit = SettleTime + TimeLimitMargin;
	TimeLimit = SettleTime + ScriptDuration + TimeLimitMargin;

	if (MaxFPS > 0.0f)
	{
		SetConsoleVariableFromFloat(TEXT("t.MaxFPS"), MaxFPS);
	}

	Character = StartOnFootClass ? SpawnOnFoot() : nullptr;
	Vehicle = Character ? nullptr : ResolveVehicle();
	Subject = Character ? static_cast<APawn*>(Character) : Vehicle;
	Movement = Vehicle ? Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovementComponent()) : nullptr;
	B1Movement = Cast<UB1VehicleMovementComponent>(Movement);
	MudVehicle = Vehicle ? Vehicle->FindComponentByClass<UB1MudVehicleComponent>() : nullptr;
	if (StartOnFootClass && !Character)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("StartOnFootClass didn't spawn, or there is no player 0"));
	}
	else if (!Character && !Movement)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("no wheeled vehicle to drive"));
	}
	else if (Character && (TestForce != EB1TestForce::Off || WinchAnchor || bDisableVehicleSleep))
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("TestForce, WinchAnchor and bDisableVehicleSleep need a vehicle"));
	}
	else if (TestForce != EB1TestForce::Off && !B1Movement)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("test force needs a UB1VehicleMovementComponent"));
	}
	else if (bDisableVehicleSleep)
	{
		Movement->SleepThreshold = 0.0f;
		Movement->SetSleeping(false);
	}
}

AWheeledVehiclePawn* AB1GymTestRig::ResolveVehicle()
{
	if (TargetVehicle)
	{
		return TargetVehicle;
	}

	if (bUsePlayerVehicle)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		return PC ? Cast<AWheeledVehiclePawn>(PC->GetPawn()) : nullptr;
	}

	if (!VehicleClass)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AWheeledVehiclePawn* Spawned = GetWorld()->SpawnActor<AWheeledVehiclePawn>(VehicleClass, GetActorTransform(), Params);
	if (!Spawned)
	{
		return nullptr;
	}

	RegisterAutoDestroyActor(Spawned);
	if (bPossessWithPlayer)
	{
		PossessWithPlayer(Spawned);
	}
	else
	{
		// Nobody possesses the spawned vehicle, so Chaos must accept inputs without a controller.
		Spawned->GetVehicleMovementComponent()->SetRequiresControllerForInputs(false);
	}
	return Spawned;
}

AB1Character* AB1GymTestRig::SpawnOnFoot()
{
	if (!GetWorld()->GetFirstPlayerController())
	{
		return nullptr;
	}

	// The rig marks the ground; the capsule centre goes half its height above it.
	const float HalfHeight = StartOnFootClass->GetDefaultObject<AB1Character>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AB1Character* Spawned = GetWorld()->SpawnActor<AB1Character>(StartOnFootClass,
		GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + 2.0f), FRotator(0.0f, GetActorRotation().Yaw, 0.0f), Params);
	if (!Spawned)
	{
		return nullptr;
	}

	RegisterAutoDestroyActor(Spawned);
	PossessWithPlayer(Spawned);
	GetWorld()->GetFirstPlayerController()->SetControlRotation(Spawned->GetActorRotation());
	return Spawned;
}

void AB1GymTestRig::PossessWithPlayer(APawn* NewPawn)
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PreviousPlayerPawn = PC->GetPawn();
		PC->Possess(NewPawn);
	}
}

bool AB1GymTestRig::IsReady_Implementation()
{
	return Subject && TotalTime >= SettleTime;
}

void AB1GymTestRig::StartTest()
{
	Super::StartTest();

	bScriptRunning = true;
	ScriptTime = 0.0f;
	NumFrames = 0;
	StartLocation = LastLocation = Subject->GetActorLocation();
	CharZmin = Character ? Character->GetCapsuleBottomZ() : 0.0f;
	PathLength = 0.0f;
	MaxSpeed = 0.0f;
	StuckTimer = 0.0f;
	TimeToStuck = -1.0f;
	SettleTimeMeasured = 0.0f;
	PeakOscillation = 0.0f;
	MaxHeightZ = StartLocation.Z;
	MaxSinkDepth.Init(0.0f, MudVehicle ? MudVehicle->GetNumWheels() : 0);
	MaxBellyDrag = 0.0f;
	MaxSlopePull = 0.0f;
	EndSpeed = 0.0f;
	LastStepMaxSpeed = 0.0f;
	bInLastStep = false;
	LastStepStart = StartLocation;
	TautTime = -1.0f;
	StretchFirst = FFloatInterval();
	StretchLast = FFloatInterval();

	MaxTension = 0.0f;
	LastStepTension = FFloatInterval();
	MaxAnchorDistance = 0.0f;

	if (TestForce != EB1TestForce::Off)
	{
		B1Movement->SetTestForce(TestForce);
	}

	Winch = Vehicle ? Vehicle->FindComponentByClass<UB1WinchComponent>() : nullptr;
	bHookUsed = false;
	MaxCarryDistance = 0.0f;
	if (WinchAnchor)
	{
		UB1WinchAnchorComponent* Anchor = WinchAnchor->FindComponentByClass<UB1WinchAnchorComponent>();
		if (!Winch || !Anchor)
		{
			FinishTest(EFunctionalTestResult::Error, TEXT("WinchAnchor needs a UB1WinchComponent on the vehicle and a UB1WinchAnchorComponent on the anchor"));
			return;
		}
		if (!Winch->AttachTo(Anchor))
		{
			FinishTest(EFunctionalTestResult::Error, TEXT("WinchAnchor is out of the cable's reach"));
			return;
		}
		AnchorStart = WinchAnchor->GetActorLocation();
		AnchorPullDirection = (Winch->GetComponentLocation() - Anchor->GetComponentLocation()).GetSafeNormal();
	}
	if (!Winch && Thresholds.ExpectHook != EB1GymExpectHook::Any)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("ExpectHook needs a UB1WinchComponent on the vehicle"));
		return;
	}

	TimeInMud = 0.0f;
	AbilitySystem = Cast<UB1AbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Subject));
	if (bPossessWithPlayer || Character)
	{
		if (!AbilitySystem)
		{
			FinishTest(EFunctionalTestResult::Error, TEXT("the player's pawn has no B1 ASC (game mode isn't AB1GameMode?)"));
			return;
		}
		const float MaxFuel = AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetMaxFuelAttribute());
		const float TargetFuel = StartFuel < 0.0f ? MaxFuel : FMath::Min(StartFuel, MaxFuel);
		AbilitySystem->AddVehicleFuel(TargetFuel - AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute()));
		FuelAtStart = AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute());
		LastFuel = FuelAtStart;
		FuelOutside = 0.0f;
		AvatarFailures.Reset();

		AbilityActivations = 0;
		AbilityActivatedHandle = AbilitySystem->AbilityActivatedCallbacks.AddUObject(this, &AB1GymTestRig::OnAbilityActivated);
	}
	else if (InputScript.ContainsByPredicate([](const FB1GymInputStep& Step) { return Step.AbilityInputTag.IsValid(); }))
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("AbilityInputTag steps need bPossessWithPlayer"));
		return;
	}
	if (!Cast<AB1Jeep>(Vehicle) && InputScript.ContainsByPredicate([](const FB1GymInputStep& Step) { return Step.bParked; }))
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("bParked steps need an AB1Jeep"));
		return;
	}
	if (!Character && !bPossessWithPlayer && InputScript.ContainsByPredicate([](const FB1GymInputStep& Step)
		{ return !Step.Move.IsZero() || Step.TeleportPawnTo || Step.bTeleportToVehicle; }))
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("Move and teleport steps need StartOnFootClass or bPossessWithPlayer"));
		return;
	}

	CurrentStep = INDEX_NONE;
	bRigParked = false;
	if (InputScript.Num() > 0)
	{
		EnterStep(0);
		ApplyInput(InputScript[0]);
	}
}

void AB1GymTestRig::EnterStep(int32 StepIndex)
{
	if (StepIndex == CurrentStep)
	{
		return;
	}

	if (CurrentStep != INDEX_NONE)
	{
		CheckExpectAvatar(CurrentStep);
	}
	CurrentStep = StepIndex;
	const FB1GymInputStep& Step = InputScript[StepIndex];
	if (Step.bUnstuck && MudVehicle)
	{
		MudVehicle->Unstuck();
	}
	if (Step.TeleportPawnTo || Step.bTeleportToVehicle)
	{
		TeleportPlayerPawn(Step);
	}
	SetAbilityInput(Step.AbilityInputTag);
}

void AB1GymTestRig::TeleportPlayerPawn(const FB1GymInputStep& Step)
{
	const AActor* Target = Step.bTeleportToVehicle ? static_cast<const AActor*>(Vehicle) : Step.TeleportPawnTo.Get();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Target || !Pawn)
	{
		UE_LOG(LogB1Test, Warning, TEXT("%s step %d: nothing to teleport or no target"), *GetName(), CurrentStep);
		return;
	}

	const FRotator TargetYaw(0.0f, Target->GetActorRotation().Yaw, 0.0f);
	const FVector Location = Target->GetActorLocation() + TargetYaw.RotateVector(Step.TeleportOffset);
	const FRotator Facing(0.0f, (Target->GetActorLocation() - Location).Rotation().Yaw, 0.0f);
	if (!Pawn->TeleportTo(Location, Facing))
	{
		UE_LOG(LogB1Test, Warning, TEXT("%s step %d: no room for %s at %s"), *GetName(), CurrentStep, *Pawn->GetName(), *Location.ToCompactString());
		return;
	}
	PC->SetControlRotation(Facing);
}

AB1Character* AB1GymTestRig::GetWalker() const
{
	if (Character || !bPossessWithPlayer)
	{
		return Character;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? Cast<AB1Character>(PC->GetPawn()) : nullptr;
}

void AB1GymTestRig::SetAbilityInput(const FGameplayTag& InputTag)
{
	if (InputTag == HeldAbilityInputTag)
	{
		return;
	}

	AB1PlayerController* PC = Cast<AB1PlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC)
	{
		HeldAbilityInputTag = FGameplayTag();
		return;
	}

	if (HeldAbilityInputTag.IsValid())
	{
		PC->AbilityInputTagReleased(HeldAbilityInputTag);
	}
	HeldAbilityInputTag = InputTag;
	if (HeldAbilityInputTag.IsValid())
	{
		PC->AbilityInputTagPressed(HeldAbilityInputTag);
	}
}

void AB1GymTestRig::CheckExpectAvatar(int32 StepIndex)
{
	const EB1GymExpectAvatar Expect = InputScript[StepIndex].ExpectAvatar;
	if (Expect == EB1GymExpectAvatar::Any)
	{
		return;
	}

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const bool bInVehicle = Expect == EB1GymExpectAvatar::InVehicle;
	const FGameplayTag& Tag = bInVehicle ? B1GameplayTags::State_Avatar_InVehicle : B1GameplayTags::State_Avatar_OnFoot;
	const bool bPawnOk = bInVehicle ? (Pawn && Pawn == Vehicle) : (Pawn && Pawn->IsA<AB1Character>());
	if (!bPawnOk || !AbilitySystem || !AbilitySystem->HasMatchingGameplayTag(Tag))
	{
		AvatarFailures.Add(StepIndex);
		UE_LOG(LogB1Test, Warning, TEXT("%s step %d: expected %s, pawn %s"), *GetName(), StepIndex,
			*UEnum::GetValueAsString(Expect), *GetNameSafe(Pawn));
	}
}

void AB1GymTestRig::OnAbilityActivated(UGameplayAbility* Ability)
{
	++AbilityActivations;
}

void AB1GymTestRig::FinishTest(EFunctionalTestResult TestResult, const FString& Message)
{
	SetAbilityInput(FGameplayTag());
	if (AbilitySystem)
	{
		AbilitySystem->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
	}
	AbilityActivatedHandle.Reset();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PreviousPlayerPawn && Subject && PC->GetPawn() == Subject)
	{
		// The template controller respawns a vehicle when its pawn is destroyed; the rig's pawn is about to be.
		Subject->OnDestroyed.RemoveAll(PC);
		PC->Possess(PreviousPlayerPawn);

		if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(B1GameplayTags::Vehicle_State_InMud))
		{
			UE_LOG(LogB1Test, Error, TEXT("%s: Vehicle.State.InMud still on the ASC after unpossess"), *GetName());
		}
	}
	PreviousPlayerPawn = nullptr;

	if (Winch)
	{
		Winch->Detach();
		Winch = nullptr;
	}

	Super::FinishTest(TestResult, Message);
}

void AB1GymTestRig::Tick(float DeltaSeconds)
{
	// StartTest runs inside Super::Tick; sampling starts on the following frame.
	const bool bScriptWasRunning = bScriptRunning;

	Super::Tick(DeltaSeconds);

	if (!bScriptWasRunning || !bIsRunning || !Subject)
	{
		return;
	}

	ScriptTime += DeltaSeconds;
	++NumFrames;
	Sample(DeltaSeconds);
	if (TestForce == EB1TestForce::Spring)
	{
		SampleSpring();
	}

	if (ScriptTime >= ScriptDuration)
	{
		FinishScript();
		return;
	}

	float StepEnd = 0.0f;
	for (int32 StepIndex = 0; StepIndex < InputScript.Num(); ++StepIndex)
	{
		StepEnd += InputScript[StepIndex].Duration;
		if (ScriptTime < StepEnd)
		{
			EnterStep(StepIndex);
			ApplyInput(InputScript[StepIndex]);
			if (!bInLastStep && StepIndex == InputScript.Num() - 1)
			{
				bInLastStep = true;
				LastStepStart = LastLocation;
			}
			break;
		}
	}
}

void AB1GymTestRig::ApplyInput(const FB1GymInputStep& Step)
{
	if (AB1Character* Walker = GetWalker())
	{
		Walker->AddMovementInput(Walker->GetActorForwardVector(), Step.Move.X);
		Walker->AddMovementInput(Walker->GetActorRightVector(), Step.Move.Y);
	}
	if (Character)
	{
		return;
	}

	Movement->SetThrottleInput(Step.Throttle);
	Movement->SetSteeringInput(Step.Steer);
	Movement->SetBrakeInput(Step.Brake);
	Movement->SetHandbrakeInput(Step.bHandbrake);
	// Only on a change, so the rig doesn't unpark a jeep the player has just left.
	AB1Jeep* Jeep = Cast<AB1Jeep>(Vehicle);
	if (Jeep && Step.bParked != bRigParked)
	{
		Jeep->SetParked(Step.bParked);
		bRigParked = Step.bParked;
	}
	if (Winch)
	{
		Winch->SetPull(Step.WinchPull);
	}
}

void AB1GymTestRig::Sample(float DeltaSeconds)
{
	const FVector Location = Subject->GetActorLocation();
	PathLength += FVector::Dist(Location, LastLocation) / CmPerM;
	LastLocation = Location;
	MaxHeightZ = FMath::Max(MaxHeightZ, static_cast<float>(Location.Z));
	if (Character)
	{
		CharZmin = FMath::Min(CharZmin, Character->GetCapsuleBottomZ());
	}

	const FVector Velocity = (Vehicle ? Vehicle->GetMesh()->GetPhysicsLinearVelocity() : Subject->GetVelocity()) / CmPerM;
	const float Speed = Velocity.Size();
	MaxSpeed = FMath::Max(MaxSpeed, Speed);
	EndSpeed = Speed;
	if (bInLastStep)
	{
		LastStepMaxSpeed = FMath::Max(LastStepMaxSpeed, Speed);
	}
	PeakOscillation = FMath::Max(PeakOscillation, FMath::Abs(static_cast<float>(Velocity.Z)));

	if (Speed > SettleSpeed)
	{
		SettleTimeMeasured = ScriptTime;
	}

	float SlopePull = 0.0f;
	for (int32 WheelIndex = 0; WheelIndex < MaxSinkDepth.Num(); ++WheelIndex)
	{
		MaxSinkDepth[WheelIndex] = FMath::Max(MaxSinkDepth[WheelIndex], MudVehicle->GetWheelSinkDepth(WheelIndex));
		SlopePull += MudVehicle->GetWheelSlopePull(WheelIndex);
	}
	MaxSlopePull = FMath::Max(MaxSlopePull, SlopePull);
	if (MudVehicle)
	{
		MaxBellyDrag = FMath::Max(MaxBellyDrag, MudVehicle->GetBellyDrag());
	}
	if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(B1GameplayTags::Vehicle_State_InMud))
	{
		TimeInMud += DeltaSeconds;
	}
	if (AbilitySystem && Vehicle)
	{
		const float Fuel = AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute());
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (!PC || PC->GetPawn() != Vehicle)
		{
			FuelOutside += FMath::Max(0.0f, LastFuel - Fuel);
		}
		LastFuel = Fuel;
	}

	if (Winch)
	{
		const EB1WinchHook Hook = Winch->GetHookState();
		bHookUsed |= Hook != EB1WinchHook::Stowed;
		if (Hook == EB1WinchHook::Carried)
		{
			MaxCarryDistance = FMath::Max(MaxCarryDistance, Winch->GetCurrentLength());
		}
		else if (Hook == EB1WinchHook::Attached)
		{
			MaxAnchorDistance = FMath::Max(MaxAnchorDistance, Winch->GetCurrentLength());
		}

		const float Tension = Winch->GetTension();
		MaxTension = FMath::Max(MaxTension, Tension);
		if (bInLastStep)
		{
			LastStepTension.Include(Tension);
		}
	}

	StuckTimer = Speed < StuckSpeed ? StuckTimer + DeltaSeconds : 0.0f;
	if (TimeToStuck < 0.0f && StuckTimer >= StuckHoldTime)
	{
		TimeToStuck = ScriptTime - StuckHoldTime;
	}
}

void AB1GymTestRig::SampleSpring()
{
	const float Distance = FVector::Dist(LastLocation, B1Movement->GetTestForceAnchor()) / CmPerM;
	const float Stretch = Distance - B1Movement->TestForceSettings.SpringRestLength;

	if (TautTime < 0.0f && Stretch > 0.0f)
	{
		TautTime = ScriptTime;
	}
	if (TautTime >= 0.0f && ScriptTime <= TautTime + 1.0f)
	{
		StretchFirst.Include(Stretch);
	}
	if (ScriptTime >= ScriptDuration - 1.0f)
	{
		StretchLast.Include(Stretch);
	}
}

void AB1GymTestRig::FinishScript()
{
	bScriptRunning = false;
	if (CurrentStep != INDEX_NONE)
	{
		CheckExpectAvatar(CurrentStep);
	}
	ApplyInput(FB1GymInputStep());
	SetAbilityInput(FGameplayTag());
	if (TestForce != EB1TestForce::Off)
	{
		B1Movement->SetTestForce(EB1TestForce::Off);
	}

	const float Displacement = FVector::Dist(LastLocation, StartLocation) / CmPerM;
	const float LastStepDisplacement = FVector::Dist2D(LastLocation, LastStepStart) / CmPerM;
	const float EndHeight = (LastLocation.Z - GetActorLocation().Z) / CmPerM;
	const float PeakHeight = (MaxHeightZ - GetActorLocation().Z) / CmPerM;
	const float AvgDt = NumFrames > 0 ? ScriptTime / NumFrames : 0.0f;

	// Still above the settle speed on the last sample means it never settled.
	if (SettleTimeMeasured >= ScriptTime)
	{
		SettleTimeMeasured = -1.0f;
	}
	const bool bSettled = SettleTimeMeasured >= 0.0f;

	TArray<FString> Failures;
	if (Thresholds.MinDistance >= 0.0f && PathLength < Thresholds.MinDistance)
	{
		Failures.Add(FString::Printf(TEXT("dist<%.2f"), Thresholds.MinDistance));
	}
	if (Thresholds.MaxDistance >= 0.0f && PathLength > Thresholds.MaxDistance)
	{
		Failures.Add(FString::Printf(TEXT("dist>%.2f"), Thresholds.MaxDistance));
	}
	if (Thresholds.MaxTimeToStuck >= 0.0f && (TimeToStuck < 0.0f || TimeToStuck > Thresholds.MaxTimeToStuck))
	{
		Failures.Add(FString::Printf(TEXT("stuck>%.2f"), Thresholds.MaxTimeToStuck));
	}
	if (Thresholds.MaxLastStepDisplacement >= 0.0f && LastStepDisplacement > Thresholds.MaxLastStepDisplacement)
	{
		Failures.Add(FString::Printf(TEXT("lastDisp>%.3f"), Thresholds.MaxLastStepDisplacement));
	}
	if (Thresholds.MaxEndSpeed >= 0.0f && EndSpeed > Thresholds.MaxEndSpeed)
	{
		Failures.Add(FString::Printf(TEXT("vend>%.2f"), Thresholds.MaxEndSpeed));
	}
	if (Thresholds.MaxLastStepSpeed >= 0.0f && LastStepMaxSpeed > Thresholds.MaxLastStepSpeed)
	{
		Failures.Add(FString::Printf(TEXT("vmaxLast>%.2f"), Thresholds.MaxLastStepSpeed));
	}
	if (Thresholds.MaxPeakOscillation >= 0.0f && PeakOscillation > Thresholds.MaxPeakOscillation)
	{
		Failures.Add(FString::Printf(TEXT("peakOsc>%.3f"), Thresholds.MaxPeakOscillation));
	}
	if (Thresholds.MaxSettleTime >= 0.0f && (!bSettled || SettleTimeMeasured > Thresholds.MaxSettleTime))
	{
		Failures.Add(FString::Printf(TEXT("settle>%.2f"), Thresholds.MaxSettleTime));
	}
	if (Thresholds.bCheckEndHeight && EndHeight < Thresholds.MinEndHeight)
	{
		Failures.Add(FString::Printf(TEXT("endH<%.2f"), Thresholds.MinEndHeight));
	}
	if (Thresholds.bCheckPeakHeight && PeakHeight > Thresholds.MaxPeakHeight)
	{
		Failures.Add(FString::Printf(TEXT("maxH>%.2f"), Thresholds.MaxPeakHeight));
	}

	FString SpringMetrics;
	if (TestForce == EB1TestForce::Spring)
	{
		const bool bTaut = TautTime >= 0.0f;
		const float SettleAfterTaut = bTaut && bSettled ? FMath::Max(0.0f, SettleTimeMeasured - TautTime) : -1.0f;
		const float AmpFirst = StretchFirst.IsValid() ? StretchFirst.Size() : 0.0f;
		const float AmpLast = StretchLast.IsValid() ? StretchLast.Size() : 0.0f;
		const float AmpGrowth = AmpFirst > 0.0f ? AmpLast / AmpFirst : 0.0f;

		if (Thresholds.MaxSpringSettle >= 0.0f && (SettleAfterTaut < 0.0f || SettleAfterTaut > Thresholds.MaxSpringSettle))
		{
			Failures.Add(FString::Printf(TEXT("settleTaut>%.2f"), Thresholds.MaxSpringSettle));
		}
		if (Thresholds.MaxSpringAmpGrowth >= 0.0f && (!bTaut || AmpGrowth > Thresholds.MaxSpringAmpGrowth))
		{
			Failures.Add(FString::Printf(TEXT("growth>%.2f"), Thresholds.MaxSpringAmpGrowth));
		}

		SpringMetrics = FString::Printf(TEXT(" taut=%.2f settleTaut=%.2f ampFirst=%.3f ampLast=%.3f growth=%.2f"),
			TautTime, SettleAfterTaut, AmpFirst, AmpLast, AmpGrowth);
	}

	FString SinkMetrics;
	if (MaxSinkDepth.Num() > 0)
	{
		TArray<FString> PerWheel;
		for (const float Depth : MaxSinkDepth)
		{
			PerWheel.Add(FString::Printf(TEXT("%.1f"), Depth));
		}
		SinkMetrics = TEXT(" sink=") + FString::Join(PerWheel, TEXT("/")) + FString::Printf(TEXT(" belly=%.0f pull=%.0f"), MaxBellyDrag, MaxSlopePull);
	}

	FString WinchMetrics;
	if (Winch && (WinchAnchor || bHookUsed || Thresholds.ExpectHook != EB1GymExpectHook::Any))
	{
		const FString HookName = StaticEnum<EB1WinchHook>()->GetNameStringByValue(static_cast<int64>(Winch->GetHookState()));
		const float CarryDistance = MaxCarryDistance / CmPerM;
		if (Thresholds.ExpectHook != EB1GymExpectHook::Any && StaticEnum<EB1GymExpectHook>()->GetNameStringByValue(static_cast<int64>(Thresholds.ExpectHook)) != HookName)
		{
			Failures.Add(FString::Printf(TEXT("hook!=%s"), *UEnum::GetValueAsString(Thresholds.ExpectHook)));
		}
		if (Thresholds.MaxCarryDistance >= 0.0f && CarryDistance > Thresholds.MaxCarryDistance)
		{
			Failures.Add(FString::Printf(TEXT("carryMax>%.2f"), Thresholds.MaxCarryDistance));
		}
		WinchMetrics = FString::Printf(TEXT(" hook=%s carryMax=%.2f"), *HookName, CarryDistance);

		const float TensionOscillation = LastStepTension.IsValid() ? LastStepTension.Size() : 0.0f;
		const float TensionMinLast = LastStepTension.IsValid() ? LastStepTension.Min : 0.0f;
		const float AnchorDistance = MaxAnchorDistance / CmPerM;
		const float RestLength = Winch->GetRestLength() / CmPerM;
		if (Thresholds.MaxTension >= 0.0f && MaxTension > Thresholds.MaxTension)
		{
			Failures.Add(FString::Printf(TEXT("tensionMax>%.0f"), Thresholds.MaxTension));
		}
		if (Thresholds.MaxTensionOscillation >= 0.0f && TensionOscillation > Thresholds.MaxTensionOscillation)
		{
			Failures.Add(FString::Printf(TEXT("tensionOsc>%.0f"), Thresholds.MaxTensionOscillation));
		}
		if (Thresholds.MaxAnchorDistance >= 0.0f && AnchorDistance > Thresholds.MaxAnchorDistance)
		{
			Failures.Add(FString::Printf(TEXT("anchorDistMax>%.2f"), Thresholds.MaxAnchorDistance));
		}

		const float AnchorTravel = WinchAnchor ? ((WinchAnchor->GetActorLocation() - AnchorStart) | AnchorPullDirection) / CmPerM : 0.0f;
		if (Thresholds.MinAnchorTravel >= 0.0f && AnchorTravel < Thresholds.MinAnchorTravel)
		{
			Failures.Add(FString::Printf(TEXT("anchorTravel<%.2f"), Thresholds.MinAnchorTravel));
		}

		if (Thresholds.MinLastStepTension >= 0.0f && TensionMinLast < Thresholds.MinLastStepTension)
		{
			Failures.Add(FString::Printf(TEXT("tensionMinLast<%.0f"), Thresholds.MinLastStepTension));
		}
		if (Thresholds.MaxRestLength >= 0.0f && RestLength > Thresholds.MaxRestLength)
		{
			Failures.Add(FString::Printf(TEXT("rest>%.2f"), Thresholds.MaxRestLength));
		}

		WinchMetrics += FString::Printf(TEXT(" tensionMax=%.0f tensionOsc=%.0f tensionMinLast=%.0f anchorDistMax=%.2f rest=%.2f vmaxLast=%.2f anchorTravel=%.2f"),
			MaxTension, TensionOscillation, TensionMinLast, AnchorDistance, RestLength, LastStepMaxSpeed, AnchorTravel);
		Winch->Detach();
		Winch = nullptr;
	}

	FString FuelMetrics;
	if (AbilitySystem)
	{
		const float FuelUsed = FuelAtStart - AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute());
		const float FuelPer100m = PathLength > 0.0f ? FuelUsed / PathLength * 100.0f : -1.0f;
		const float FuelPerHour = ScriptTime > 0.0f ? FuelUsed / ScriptTime * 3600.0f : 0.0f;

		if (Thresholds.MinFuelPer100m >= 0.0f && FuelPer100m < Thresholds.MinFuelPer100m)
		{
			Failures.Add(FString::Printf(TEXT("fuelPer100m<%.3f"), Thresholds.MinFuelPer100m));
		}
		if (Thresholds.MaxFuelPer100m >= 0.0f && FuelPer100m > Thresholds.MaxFuelPer100m)
		{
			Failures.Add(FString::Printf(TEXT("fuelPer100m>%.3f"), Thresholds.MaxFuelPer100m));
		}

		if (Thresholds.MaxFuelUsed >= 0.0f && FuelUsed > Thresholds.MaxFuelUsed)
		{
			Failures.Add(FString::Printf(TEXT("fuelUsed>%.4f"), Thresholds.MaxFuelUsed));
		}
		if (Thresholds.MaxFuelOutside >= 0.0f && FuelOutside > Thresholds.MaxFuelOutside)
		{
			Failures.Add(FString::Printf(TEXT("fuelOutside>%.4f"), Thresholds.MaxFuelOutside));
		}
		if (!AvatarFailures.IsEmpty())
		{
			Failures.Add(FString::Printf(TEXT("avatar@%s"), *FString::JoinBy(AvatarFailures, TEXT("/"), [](int32 Step) { return FString::FromInt(Step); })));
		}
		if (Thresholds.MinFuelPerHour >= 0.0f && FuelPerHour < Thresholds.MinFuelPerHour)
		{
			Failures.Add(FString::Printf(TEXT("fuelPerHour<%.1f"), Thresholds.MinFuelPerHour));
		}

		if (Thresholds.MinInMudTime >= 0.0f && TimeInMud < Thresholds.MinInMudTime)
		{
			Failures.Add(FString::Printf(TEXT("inMud<%.2f"), Thresholds.MinInMudTime));
		}

		if (Thresholds.MinAbilityActivations >= 0 && AbilityActivations < Thresholds.MinAbilityActivations)
		{
			Failures.Add(FString::Printf(TEXT("abilities<%d"), Thresholds.MinAbilityActivations));
		}
		if (Thresholds.MaxAbilityActivations >= 0 && AbilityActivations > Thresholds.MaxAbilityActivations)
		{
			Failures.Add(FString::Printf(TEXT("abilities>%d"), Thresholds.MaxAbilityActivations));
		}

		FuelMetrics = FString::Printf(TEXT(" fuelUsed=%.3f fuelPer100m=%.3f fuelPerHour=%.1f cond=%.1f inMud=%.2f abilities=%d"), FuelUsed, FuelPer100m, FuelPerHour,
			AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute()), TimeInMud, AbilityActivations);
		if (Vehicle)
		{
			FuelMetrics += FString::Printf(TEXT(" fuelOutside=%.4f"), FuelOutside);
		}
	}

	FString PlayerMetrics;
	if (bPossessWithPlayer || Character)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const APawn* PossessedPawn = PC ? PC->GetPawn() : nullptr;
		const TCHAR* Avatar = TEXT("none");
		if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(B1GameplayTags::State_Avatar_InVehicle))
		{
			Avatar = TEXT("InVehicle");
		}
		else if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(B1GameplayTags::State_Avatar_OnFoot))
		{
			Avatar = TEXT("OnFoot");
		}
		PlayerMetrics = FString::Printf(TEXT(" pawn=%s avatar=%s"), PossessedPawn ? *PossessedPawn->GetClass()->GetName() : TEXT("none"), Avatar);
	}
	if (bPossessWithPlayer && Vehicle)
	{
		TArray<AActor*> Attached;
		Vehicle->GetAttachedActors(Attached);
		const bool bSeated = Attached.ContainsByPredicate([](const AActor* Actor) { return Actor->IsA<AB1Character>(); });
		PlayerMetrics += FString::Printf(TEXT(" seated=%d"), bSeated ? 1 : 0);
	}
	if (Character)
	{
		const float CharZminM = (CharZmin - GetActorLocation().Z) / CmPerM;
		if (Thresholds.bCheckCharZmin && (CharZminM < Thresholds.CharZminRange.Min || CharZminM > Thresholds.CharZminRange.Max))
		{
			Failures.Add(FString::Printf(TEXT("charZmin!=%.3f..%.3f"), Thresholds.CharZminRange.Min, Thresholds.CharZminRange.Max));
		}
		PlayerMetrics += FString::Printf(TEXT(" charZmin=%.3f"), CharZminM);
	}

	const bool bPassed = Failures.Num() == 0;
	const FString Name = TestLabel.IsEmpty() ? GetName() : TestLabel;
	const FString Line = FString::Printf(TEXT("B1Test %s %s dist=%.2f disp=%.2f vmax=%.2f vend=%.2f lastDisp=%.3f stuck=%.2f settle=%.2f peakOsc=%.3f endH=%.2f maxH=%.2f avgDt=%.4f frames=%d%s%s%s%s%s%s%s"),
		*Name, bPassed ? TEXT("PASS") : TEXT("FAIL"),
		PathLength, Displacement, MaxSpeed, EndSpeed, LastStepDisplacement, TimeToStuck, SettleTimeMeasured, PeakOscillation, EndHeight, PeakHeight, AvgDt, NumFrames, *SinkMetrics, *SpringMetrics, *WinchMetrics, *FuelMetrics, *PlayerMetrics,
		bPassed ? TEXT("") : TEXT(" failed="), *FString::Join(Failures, TEXT(",")));

	UE_LOG(LogB1Test, Display, TEXT("%s"), *Line);
	FinishTest(bPassed ? EFunctionalTestResult::Succeeded : EFunctionalTestResult::Failed, Line);
}
