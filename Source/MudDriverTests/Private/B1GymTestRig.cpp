#include "B1GymTestRig.h"
#include "B1Log.h"
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
	if (!Movement)
	{
		FinishTest(EFunctionalTestResult::Error, TEXT("no wheeled vehicle to drive"));
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
	if (Spawned)
	{
		RegisterAutoDestroyActor(Spawned);
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

	if (InputScript.Num() > 0)
	{
		ApplyInput(InputScript[0]);
	}
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

	if (ScriptTime >= ScriptDuration)
	{
		FinishScript();
		return;
	}

	float StepEnd = 0.0f;
	for (const FB1GymInputStep& Step : InputScript)
	{
		StepEnd += Step.Duration;
		if (ScriptTime < StepEnd)
		{
			ApplyInput(Step);
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
	PeakOscillation = FMath::Max(PeakOscillation, FMath::Abs(static_cast<float>(Velocity.Z)));

	if (Speed > SettleSpeed)
	{
		SettleTimeMeasured = ScriptTime;
	}

	StuckTimer = Speed < StuckSpeed ? StuckTimer + DeltaSeconds : 0.0f;
	if (TimeToStuck < 0.0f && StuckTimer >= StuckHoldTime)
	{
		TimeToStuck = ScriptTime - StuckHoldTime;
	}
}

void AB1GymTestRig::FinishScript()
{
	bScriptRunning = false;
	ApplyInput(FB1GymInputStep());

	const float Displacement = FVector::Dist(LastLocation, StartLocation) / CmPerM;
	const float AvgDt = NumFrames > 0 ? ScriptTime / NumFrames : 0.0f;

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
	if (Thresholds.MaxPeakOscillation >= 0.0f && PeakOscillation > Thresholds.MaxPeakOscillation)
	{
		Failures.Add(FString::Printf(TEXT("peakOsc>%.3f"), Thresholds.MaxPeakOscillation));
	}
	if (Thresholds.MaxSettleTime >= 0.0f && SettleTimeMeasured > Thresholds.MaxSettleTime)
	{
		Failures.Add(FString::Printf(TEXT("settle>%.2f"), Thresholds.MaxSettleTime));
	}

	const bool bPassed = Failures.Num() == 0;
	const FString Name = TestLabel.IsEmpty() ? GetName() : TestLabel;
	const FString Line = FString::Printf(TEXT("B1Test %s %s dist=%.2f disp=%.2f vmax=%.2f stuck=%.2f settle=%.2f peakOsc=%.3f avgDt=%.4f frames=%d%s%s"),
		*Name, bPassed ? TEXT("PASS") : TEXT("FAIL"),
		PathLength, Displacement, MaxSpeed, TimeToStuck, SettleTimeMeasured, PeakOscillation, AvgDt, NumFrames,
		bPassed ? TEXT("") : TEXT(" failed="), *FString::Join(Failures, TEXT(",")));

	UE_LOG(LogB1Test, Display, TEXT("%s"), *Line);
	FinishTest(bPassed ? EFunctionalTestResult::Succeeded : EFunctionalTestResult::Failed, Line);
}
