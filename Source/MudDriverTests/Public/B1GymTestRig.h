#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "B1VehicleSimInput.h"
#include "B1GymTestRig.generated.h"

class AWheeledVehiclePawn;
class UChaosWheeledVehicleMovementComponent;
class UB1VehicleMovementComponent;
class UB1AbilitySystemComponent;

USTRUCT(BlueprintType)
struct FB1GymInputStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "s", ClampMin = "0"))
	float Duration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (ClampMin = "0", ClampMax = "1"))
	float Throttle = 0.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (ClampMin = "-1", ClampMax = "1"))
	float Steer = 0.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (ClampMin = "0", ClampMax = "1"))
	float Brake = 0.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	bool bHandbrake = false;

	/** Runs B1Unstuck when the step starts; needs a UB1VehicleMovementComponent */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	bool bUnstuck = false;
};

/** Pass criteria. Negative values disable a check */
USTRUCT(BlueprintType)
struct FB1GymThresholds
{
	GENERATED_BODY()

	/** Path length */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MinDistance = -1.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxDistance = -1.0f;

	/** The vehicle must get stuck within this time */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "s"))
	float MaxTimeToStuck = -1.0f;

	/** Horizontal displacement during the last input step, a creep check */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxLastStepDisplacement = -1.0f;

	/** Speed on the last sample */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m/s"))
	float MaxEndSpeed = -1.0f;

	/** Peak vertical chassis speed, a jitter / explosion indicator */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m/s"))
	float MaxPeakOscillation = -1.0f;

	/** Time from script start until the speed last exceeded the settle speed */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "s"))
	float MaxSettleTime = -1.0f;

	/** Settle time measured from the moment the spring first goes taut */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "s"))
	float MaxSpringSettle = -1.0f;

	/** Stretch amplitude in the last second over the first second after going taut; above 1 means growing */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MaxSpringAmpGrowth = -1.0f;

	/** Fuel used per 100 m of path; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MinFuelPer100m = -1.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MaxFuelPer100m = -1.0f;

	/** L/h averaged over the script; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MinFuelPerHour = -1.0f;

	/** Seconds the ASC had Vehicle.State.InMud; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "s"))
	float MinInMudTime = -1.0f;

	/** Vehicle height above the rig actor at script end; has a toggle since negative heights are valid */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m", EditCondition = "bCheckEndHeight"))
	float MinEndHeight = 0.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (InlineEditConditionToggle))
	bool bCheckEndHeight = false;
};

/**
 * Drives a vehicle through a scripted input sequence and logs one result line:
 * `B1Test <name> PASS|FAIL key=value ...` to LogB1Test.
 */
UCLASS()
class MUDDRIVERTESTS_API AB1GymTestRig : public AFunctionalTest
{
	GENERATED_BODY()

public:

	AB1GymTestRig();

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;
	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;
	virtual void FinishTest(EFunctionalTestResult TestResult, const FString& Message) override;

	/** Used if set; otherwise the player's vehicle (bUsePlayerVehicle) or a VehicleClass spawned at the rig */
	UPROPERTY(EditInstanceOnly, Category = "B1Test|Config")
	TObjectPtr<AWheeledVehiclePawn> TargetVehicle;

	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	bool bUsePlayerVehicle = false;

	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	TSubclassOf<AWheeledVehiclePawn> VehicleClass;

	/** Player 0 possesses the spawned vehicle, so it gets the PlayerState ASC (fuel, tags). One such rig per PIE round */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	bool bPossessWithPlayer = false;

	/** Litres set at script start; negative fills the tank */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (EditCondition = "bPossessWithPlayer"))
	float StartFuel = -1.0f;

	/** Time for the vehicle to settle on its suspension before the script starts */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (Units = "s"))
	float SettleTime = 1.5f;

	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	TArray<FB1GymInputStep> InputScript;

	/** Switched on when the script starts, anchored there; needs a UB1VehicleMovementComponent */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	EB1TestForce TestForce = EB1TestForce::Off;

	/** Sets t.MaxFPS for the run; 0 leaves it alone */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	float MaxFPS = 0.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	FB1GymThresholds Thresholds;

	/** Below this speed for StuckHoldTime counts as stuck */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (Units = "m/s"))
	float StuckSpeed = 0.5f;

	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (Units = "s"))
	float StuckHoldTime = 1.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (Units = "m/s"))
	float SettleSpeed = 0.1f;

	/** Starts the test on BeginPlay without the automation framework, for PIE checks */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	bool bAutoRun = false;

private:

	AWheeledVehiclePawn* ResolveVehicle();
	void ApplyInput(const FB1GymInputStep& Step);
	void EnterStep(int32 StepIndex);
	void Sample(float DeltaSeconds);
	void SampleSpring();
	void FinishScript();

	UPROPERTY(Transient)
	TObjectPtr<AWheeledVehiclePawn> Vehicle;

	UPROPERTY(Transient)
	TObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UB1VehicleMovementComponent> B1Movement;

	/** Player 0's pawn before bPossessWithPlayer took over; possessed again when the test finishes */
	UPROPERTY(Transient)
	TObjectPtr<APawn> PreviousPlayerPawn;

	UPROPERTY(Transient)
	TObjectPtr<UB1AbilitySystemComponent> AbilitySystem;

	float FuelAtStart = 0.0f;

	/** s with Vehicle.State.InMud on the ASC */
	float TimeInMud = 0.0f;

	bool bScriptRunning = false;
	int32 CurrentStep = INDEX_NONE;
	float ScriptTime = 0.0f;
	float ScriptDuration = 0.0f;
	int32 NumFrames = 0;

	FVector StartLocation = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	float PathLength = 0.0f;
	float MaxSpeed = 0.0f;
	float StuckTimer = 0.0f;
	float TimeToStuck = -1.0f;
	float SettleTimeMeasured = 0.0f;
	float PeakOscillation = 0.0f;

	/** Per wheel, cm */
	TArray<float> MaxSinkDepth;

	/** N */
	float MaxBellyDrag = 0.0f;
	float EndSpeed = 0.0f;

	FVector LastStepStart = FVector::ZeroVector;
	bool bInLastStep = false;

	float TautTime = -1.0f;
	FFloatInterval StretchFirst;
	FFloatInterval StretchLast;
};
