#include "B1GymTestRig.h"
#include "B1Log.h"
#include "B1AbilitySystemComponent.h"
#include "B1GameplayTags.h"
#include "B1VehicleAttributeSet.h"
#include "B1VehicleMovementComponent.h"
#include "B1MudVehicleComponent.h"
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

	Vehicle = ResolveVehicle();
	Movement = Vehicle ? Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovementComponent()) : nullptr;
	B1Movement = Cast<UB1VehicleMovementComponent>(Movement);
	MudVehicle = Vehicle ? Vehicle->FindComponentByClass<UB1MudVehicleComponent>() : nullptr;
	if (!Movement)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("no wheeled vehicle to drive"));
	}
	else if (TestForce != EB1TestForce::Off && !B1Movement)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("test force needs a UB1VehicleMovementComponent"));
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
	if (APlayerController* PC = bPossessWithPlayer ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PreviousPlayerPawn = PC->GetPawn();
		PC->Possess(Spawned);
	}
	else
	{
		// Nobody possesses the spawned vehicle, so Chaos must accept inputs without a controller.
		Spawned->GetVehicleMovementComponent()->SetRequiresControllerForInputs(false);
	}
	return Spawned;
}

bool AB1GymTestRig::IsReady_Implementation()
{
	return Movement && TotalTime >= SettleTime;
}

void AB1GymTestRig::StartTest()
{
	Super::StartTest();

	bScriptRunning = true;
	ScriptTime = 0.0f;
	NumFrames = 0;
	StartLocation = LastLocation = Vehicle->GetActorLocation();
	PathLength = 0.0f;
	MaxSpeed = 0.0f;
	StuckTimer = 0.0f;
	TimeToStuck = -1.0f;
	SettleTimeMeasured = 0.0f;
	PeakOscillation = 0.0f;
	MaxSinkDepth.Init(0.0f, MudVehicle ? MudVehicle->GetNumWheels() : 0);
	MaxBellyDrag = 0.0f;
	EndSpeed = 0.0f;
	bInLastStep = false;
	LastStepStart = StartLocation;
	TautTime = -1.0f;
	StretchFirst = FFloatInterval();
	StretchLast = FFloatInterval();

	if (TestForce != EB1TestForce::Off)
	{
		B1Movement->SetTestForce(TestForce);
	}

	TimeInMud = 0.0f;
	AbilitySystem = Cast<UB1AbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Vehicle));
	if (bPossessWithPlayer)
	{
		if (!AbilitySystem)
		{
			FinishTest(EFunctionalTestResult::Error, TEXT("bPossessWithPlayer but the vehicle has no B1 ASC (game mode isn't AB1GameMode?)"));
			return;
		}
		const float MaxFuel = AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetMaxFuelAttribute());
		const float TargetFuel = StartFuel < 0.0f ? MaxFuel : FMath::Min(StartFuel, MaxFuel);
		AbilitySystem->AddVehicleFuel(TargetFuel - AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute()));
		FuelAtStart = AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute());
	}

	CurrentStep = INDEX_NONE;
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

	CurrentStep = StepIndex;
	if (InputScript[StepIndex].bUnstuck && MudVehicle)
	{
		MudVehicle->Unstuck();
	}
}

void AB1GymTestRig::FinishTest(EFunctionalTestResult TestResult, const FString& Message)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PreviousPlayerPawn && Vehicle && PC->GetPawn() == Vehicle)
	{
		// The template controller respawns a vehicle when its pawn is destroyed; the rig's jeep is about to be.
		Vehicle->OnDestroyed.RemoveAll(PC);
		PC->Possess(PreviousPlayerPawn);

		if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(B1GameplayTags::Vehicle_State_InMud))
		{
			UE_LOG(LogB1Test, Error, TEXT("%s: Vehicle.State.InMud still on the ASC after unpossess"), *GetName());
		}
	}
	PreviousPlayerPawn = nullptr;

	Super::FinishTest(TestResult, Message);
}

void AB1GymTestRig::Tick(float DeltaSeconds)
{
	// StartTest runs inside Super::Tick; sampling starts on the following frame.
	const bool bScriptWasRunning = bScriptRunning;

	Super::Tick(DeltaSeconds);

	if (!bScriptWasRunning || !bIsRunning || !Movement)
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
	Movement->SetThrottleInput(Step.Throttle);
	Movement->SetSteeringInput(Step.Steer);
	Movement->SetBrakeInput(Step.Brake);
	Movement->SetHandbrakeInput(Step.bHandbrake);
}

void AB1GymTestRig::Sample(float DeltaSeconds)
{
	const FVector Location = Vehicle->GetActorLocation();
	PathLength += FVector::Dist(Location, LastLocation) / CmPerM;
	LastLocation = Location;

	const FVector Velocity = Vehicle->GetMesh()->GetPhysicsLinearVelocity() / CmPerM;
	const float Speed = Velocity.Size();
	MaxSpeed = FMath::Max(MaxSpeed, Speed);
	EndSpeed = Speed;
	PeakOscillation = FMath::Max(PeakOscillation, FMath::Abs(static_cast<float>(Velocity.Z)));

	if (Speed > SettleSpeed)
	{
		SettleTimeMeasured = ScriptTime;
	}

	for (int32 WheelIndex = 0; WheelIndex < MaxSinkDepth.Num(); ++WheelIndex)
	{
		MaxSinkDepth[WheelIndex] = FMath::Max(MaxSinkDepth[WheelIndex], MudVehicle->GetWheelSinkDepth(WheelIndex));
	}
	if (MudVehicle)
	{
		MaxBellyDrag = FMath::Max(MaxBellyDrag, MudVehicle->GetBellyDrag());
	}
	if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(B1GameplayTags::Vehicle_State_InMud))
	{
		TimeInMud += DeltaSeconds;
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
	ApplyInput(FB1GymInputStep());
	if (TestForce != EB1TestForce::Off)
	{
		B1Movement->SetTestForce(EB1TestForce::Off);
	}

	const float Displacement = FVector::Dist(LastLocation, StartLocation) / CmPerM;
	const float LastStepDisplacement = FVector::Dist2D(LastLocation, LastStepStart) / CmPerM;
	const float EndHeight = (LastLocation.Z - GetActorLocation().Z) / CmPerM;
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
		SinkMetrics = TEXT(" sink=") + FString::Join(PerWheel, TEXT("/")) + FString::Printf(TEXT(" belly=%.0f"), MaxBellyDrag);
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

		if (Thresholds.MinFuelPerHour >= 0.0f && FuelPerHour < Thresholds.MinFuelPerHour)
		{
			Failures.Add(FString::Printf(TEXT("fuelPerHour<%.1f"), Thresholds.MinFuelPerHour));
		}

		if (Thresholds.MinInMudTime >= 0.0f && TimeInMud < Thresholds.MinInMudTime)
		{
			Failures.Add(FString::Printf(TEXT("inMud<%.2f"), Thresholds.MinInMudTime));
		}

		FuelMetrics = FString::Printf(TEXT(" fuelUsed=%.3f fuelPer100m=%.3f fuelPerHour=%.1f cond=%.1f inMud=%.2f"), FuelUsed, FuelPer100m, FuelPerHour,
			AbilitySystem->GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute()), TimeInMud);
	}

	const bool bPassed = Failures.Num() == 0;
	const FString Name = TestLabel.IsEmpty() ? GetName() : TestLabel;
	const FString Line = FString::Printf(TEXT("B1Test %s %s dist=%.2f disp=%.2f vmax=%.2f vend=%.2f lastDisp=%.3f stuck=%.2f settle=%.2f peakOsc=%.3f endH=%.2f avgDt=%.4f frames=%d%s%s%s%s%s"),
		*Name, bPassed ? TEXT("PASS") : TEXT("FAIL"),
		PathLength, Displacement, MaxSpeed, EndSpeed, LastStepDisplacement, TimeToStuck, SettleTimeMeasured, PeakOscillation, EndHeight, AvgDt, NumFrames, *SinkMetrics, *SpringMetrics, *FuelMetrics,
		bPassed ? TEXT("") : TEXT(" failed="), *FString::Join(Failures, TEXT(",")));

	UE_LOG(LogB1Test, Display, TEXT("%s"), *Line);
	FinishTest(bPassed ? EFunctionalTestResult::Succeeded : EFunctionalTestResult::Failed, Line);
}
