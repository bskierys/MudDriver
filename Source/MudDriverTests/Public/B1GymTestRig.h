#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "B1VehicleSimInput.h"
#include "GameplayTagContainer.h"
#include "B1GymTestRig.generated.h"

class AWheeledVehiclePawn;
class UChaosWheeledVehicleMovementComponent;
class UB1VehicleMovementComponent;
class UB1MudVehicleComponent;
class UB1MudDirtComponent;
class UB1AbilitySystemComponent;
class UB1WinchComponent;
class UGameplayAbility;
class AB1Character;
class AB1WinchLog;

UENUM()
enum class EB1GymExpectAvatar : uint8
{
	Any,
	InVehicle,
	OnFoot,
};

UENUM()
enum class EB1GymExpectHook : uint8
{
	Any,
	Stowed,
	Carried,
	Attached,
};

UENUM()
enum class EB1GymExpectLogState : uint8
{
	Any,
	Frozen,
	Free,
};

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

	/** Steers toward this point in the rig's frame (cm) every frame instead of using Steer */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (EditCondition = "bSteerToTarget"))
	FVector SteerTarget = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (InlineEditConditionToggle))
	bool bSteerToTarget = false;

	/** Back and forth: Throttle forward, then Throttle as brake/reverse once the vehicle is this far past the step's start along its heading, and back. 0 = off */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m", ClampMin = "0"))
	float ShuttleDistance = 0.0f;

	/** Runs B1Unstuck when the step starts; needs a UB1MudVehicleComponent */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	bool bUnstuck = false;

	/** AB1Jeep::SetParked for this step: parking brake and engine off */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	bool bParked = false;

	/** Winch pull for this step, negative reels out; needs the rig's WinchAnchor */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (ClampMin = "-1", ClampMax = "1"))
	float WinchPull = 0.0f;

	/** Stows the vehicle's winch hook when the step starts */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	bool bWinchDetach = false;

	/** Pressed through player 0's controller at step start, released at its end; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Categories = "InputTag"))
	FGameplayTag AbilityInputTag;

	/** On-foot movement input, X forward and Y right relative to player 0's character; needs StartOnFootClass or bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	FVector2D Move = FVector2D::ZeroVector;

	/** At step start, teleports player 0's pawn to this actor + TeleportOffset, facing it. For long trips only */
	UPROPERTY(EditInstanceOnly, Category = "B1Test")
	TObjectPtr<AActor> TeleportPawnTo;

	/** Teleport target is the rig's vehicle instead */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	bool bTeleportToVehicle = false;

	/** In the target's yaw frame, cm */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	FVector TeleportOffset = FVector::ZeroVector;

	/** Checked when the step ends: player 0's pawn and State.Avatar tag; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	EB1GymExpectAvatar ExpectAvatar = EB1GymExpectAvatar::Any;

	/** Every frame of the step, points player 0's character view at this actor + LookAtOffset (world, cm) */
	UPROPERTY(EditInstanceOnly, Category = "B1Test")
	TObjectPtr<AActor> LookAtActor;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	FVector LookAtOffset = FVector::ZeroVector;

	/** Checked when the step ends: hook point height above the hooked anchor's actor, m */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (EditCondition = "bCheckHookHeight"))
	FFloatInterval HookHeightRange = FFloatInterval(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (InlineEditConditionToggle))
	bool bCheckHookHeight = false;
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

	/** Peak speed during the last input step */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m/s"))
	float MaxLastStepSpeed = -1.0f;

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

	/** Litres used over the script; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MaxFuelUsed = -1.0f;

	/** Litres used while player 0 isn't driving the rig's vehicle; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MaxFuelOutside = -1.0f;

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

	/** Highest vehicle height above the rig actor during the script; a climb limit */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m", EditCondition = "bCheckPeakHeight"))
	float MaxPeakHeight = 0.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (InlineEditConditionToggle))
	bool bCheckPeakHeight = false;

	/** Peak cable tension; needs WinchAnchor */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "N"))
	float MaxTension = -1.0f;

	/** Peak cable tension must reach this, e.g. the drum stall */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "N"))
	float MinTension = -1.0f;

	/** Tension range (max − min) during the last input step */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "N"))
	float MaxTensionOscillation = -1.0f;

	/** Lowest tension during the last input step; above 0 means the cable stayed taut */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "N"))
	float MinLastStepTension = -1.0f;

	/** Cable rest length at script end */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxRestLength = -1.0f;

	/** Peak winch point to anchor distance */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxAnchorDistance = -1.0f;

	/** WinchAnchor's travel toward the winch point at script start, for a movable anchor (log) */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MinAnchorTravel = -1.0f;

	/** Peak winch point to hand distance while the hook is carried */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxCarryDistance = -1.0f;

	/** Hook state at script end; Any skips the check */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	EB1GymExpectHook ExpectHook = EB1GymExpectHook::Any;

	/** Peak log displacement from its placed pose; needs TargetLog or a log as WinchAnchor */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxLogMove = -1.0f;

	/** |yaw| turned by the log at script end */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "deg"))
	float MinLogYaw = -1.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	int32 MaxLogSwitches = -1;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	EB1GymExpectLogState ExpectLogState = EB1GymExpectLogState::Any;

	/** Gap between the free-side band and the log's footprint at script end; see LogFreeBandOffset */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MinLogGap = -1.0f;

	/** For a hook spot that shouldn't open the road as far */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxLogGap = -1.0f;

	/** Time from script start until the gap first reached MinLogGap */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "s"))
	float MaxLogGapTime = -1.0f;

	/** Turn centre's distance from the root end while the log turns */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxLogPivot = -1.0f;

	/** Peak vehicle travel past the log's placed line, along the road */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MinLogCross = -1.0f;

	/** Vehicle displacement from its start at script end */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxDisplacement = -1.0f;

	/** Abilities activated on the PlayerState ASC during the script; needs bPossessWithPlayer */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	int32 MinAbilityActivations = -1;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	int32 MaxAbilityActivations = -1;

	/** Lowest capsule bottom above the rig actor during the script; needs StartOnFootClass */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m", EditCondition = "bCheckCharZmin"))
	FFloatInterval CharZminRange = FFloatInterval(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (InlineEditConditionToggle))
	bool bCheckCharZmin = false;

	/** Lowest wheel dirt at script end; needs a UB1MudDirtComponent */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MinWheelDirt = -1.0f;

	/** Highest wheel dirt at script end */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MaxWheelDirt = -1.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MinBodyDirt = -1.0f;

	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MaxBodyDirt = -1.0f;

	/** Body dirt below the lowest wheel dirt at script end */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	bool bExpectBodyBehindWheels = false;

	/** Sum of every dirt value's change during the last input step, a cap check */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MaxLastStepDirtChange = -1.0f;

	/** Lowest per-wheel peak tire coat */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MinTireCoatPeak = -1.0f;

	/** Lowest per-wheel drop from peak tire coat to the end, e.g. after firm ground */
	UPROPERTY(EditAnywhere, Category = "B1Test")
	float MinTireCoatDrop = -1.0f;

	/** Worst wheel's path from its first mud contact until its coat reached TreadCoat */
	UPROPERTY(EditAnywhere, Category = "B1Test", meta = (Units = "m"))
	float MaxTireCoatDistance = -1.0f;
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

	/** Spawns this character at the rig and player 0 possesses it instead of a vehicle */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	TSubclassOf<AB1Character> StartOnFootClass;

	/** Player 0 possesses the spawned vehicle, so it gets the PlayerState ASC (fuel, tags). One such rig per PIE round */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	bool bPossessWithPlayer = false;

	/** Litres set at script start; negative fills the tank */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (EditCondition = "bPossessWithPlayer"))
	float StartFuel = -1.0f;

	/** Every dirt value set to this at script start (UB1MudDirtComponent::SetDirt); negative leaves it */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	float StartDirt = -1.0f;

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

	/** Zeroes the vehicle's SleepThreshold, to see what the physics does without Chaos sleep holding it */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config")
	bool bDisableVehicleSleep = false;

	/** The vehicle's winch hooks this actor's anchor at script start (rest = distance); steps set WinchPull */
	UPROPERTY(EditInstanceOnly, Category = "B1Test|Config")
	TObjectPtr<AActor> WinchAnchor;

	/** Log metrics without hooking it; a log as WinchAnchor is tracked too. Reset to its placed pose at script start */
	UPROPERTY(EditInstanceOnly, Category = "B1Test|Config")
	TObjectPtr<AB1WinchLog> TargetLog;

	/** Free-side band line past the log's placed tip (+X end), along the placed trunk axis; logGap is measured to it */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (Units = "cm"))
	float LogFreeBandOffset = 30.0f;

	/** Where on WinchAnchor's area; 1/3 is 80 cm on a tree, the pre-area hook height */
	UPROPERTY(EditAnywhere, Category = "B1Test|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float WinchAnchorAlpha = 1.0f / 3.0f;

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
	AB1Character* SpawnOnFoot();
	void PossessWithPlayer(APawn* NewPawn);
	void ApplyInput(const FB1GymInputStep& Step);
	void EnterStep(int32 StepIndex);
	void Sample(float DeltaSeconds);
	void SampleSpring();
	void FinishScript();
	void SetAbilityInput(const FGameplayTag& InputTag);
	void OnAbilityActivated(UGameplayAbility* Ability);
	void CheckExpectAvatar(int32 StepIndex);
	void CheckHookHeight(int32 StepIndex);
	/** Hook point above the hooked anchor's actor, m; 0 when not attached */
	float GetHookHeight() const;
	void TeleportPlayerPawn(const FB1GymInputStep& Step);
	/** The rig's on-foot character, or the one player 0 got out as */
	AB1Character* GetWalker() const;
	void SampleLog(float DeltaSeconds);
	/** cm, free-side band line to the footprint (box corners + root ball), in the placed log's frame */
	float GetLogGap() const;
	FVector GetLogRootEnd() const;

	/** The rig's vehicle or on-foot character; what the metrics sample */
	UPROPERTY(Transient)
	TObjectPtr<APawn> Subject;

	UPROPERTY(Transient)
	TObjectPtr<AWheeledVehiclePawn> Vehicle;

	UPROPERTY(Transient)
	TObjectPtr<AB1Character> Character;

	/** Lowest capsule bottom, world cm */
	float CharZmin = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UB1VehicleMovementComponent> B1Movement;

	UPROPERTY(Transient)
	TObjectPtr<UB1MudVehicleComponent> MudVehicle;

	UPROPERTY(Transient)
	TObjectPtr<UB1MudDirtComponent> MudDirt;

	/** ShuttleDistance state, set when a step starts */
	FVector ShuttleOrigin = FVector::ZeroVector;
	FVector ShuttleAxis = FVector::ForwardVector;
	bool bShuttleReverse = false;

	/** Sum of all dirt values when the last step started; negative before it */
	float DirtAtLastStep = -1.0f;
	float GetDirtSum() const;

	/** Per wheel: peak coat, path (m) at first mud contact, path from there to TreadCoat; -1 = not yet */
	TArray<float> TireCoatPeak;
	TArray<float> TireMudStartPath;
	TArray<float> TireCoatDistance;
	void SampleTireCoat();

	/** The vehicle's winch; steps set its pull */
	UPROPERTY(Transient)
	TObjectPtr<UB1WinchComponent> Winch;

	/** Hook left the drum during the script; winch metrics print when this or WinchAnchor is set */
	bool bHookUsed = false;
	/** cm */
	float MaxCarryDistance = 0.0f;

	/** Player 0's pawn before bPossessWithPlayer took over; possessed again when the test finishes */
	UPROPERTY(Transient)
	TObjectPtr<APawn> PreviousPlayerPawn;

	UPROPERTY(Transient)
	TObjectPtr<UB1AbilitySystemComponent> AbilitySystem;

	float FuelAtStart = 0.0f;
	float LastFuel = 0.0f;
	bool bRigParked = false;
	float FuelOutside = 0.0f;

	/** Steps whose ExpectAvatar didn't hold, as step indices */
	TArray<int32> AvatarFailures;
	TArray<int32> HookHeightFailures;

	/** Ability input tag currently held through the player controller */
	FGameplayTag HeldAbilityInputTag;
	int32 AbilityActivations = 0;
	FDelegateHandle AbilityActivatedHandle;

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
	float MaxHeightZ = 0.0f;

	/** Per wheel, cm */
	TArray<float> MaxSinkDepth;

	/** N */
	float MaxBellyDrag = 0.0f;

	/** N, peak of the summed per-wheel slope pull */
	float MaxSlopePull = 0.0f;
	float EndSpeed = 0.0f;
	float LastStepMaxSpeed = 0.0f;

	FVector LastStepStart = FVector::ZeroVector;
	bool bInLastStep = false;

	/** N */
	float MaxTension = 0.0f;
	FFloatInterval LastStepTension;
	/** Peak handbrake hold, N */
	float MaxHold = 0.0f;

	/** TargetLog or a log WinchAnchor */
	UPROPERTY(Transient)
	TObjectPtr<AB1WinchLog> TrackedLog;
	/** cm */
	float MaxLogMove = 0.0f;
	float LogGapTime = -1.0f;
	/** Root end's horizontal path (cm) and yaw (rad) while turning; their ratio is the pivot distance */
	float LogRootPath = 0.0f;
	float LogTurn = 0.0f;
	FVector LastLogRootEnd = FVector::ZeroVector;
	float LastLogYaw = 0.0f;
	/** End of the last pulling step, and the log's location 5 s before it */
	float PullEndTime = -1.0f;
	FVector LogDriftStart = FVector::ZeroVector;
	bool bLogDriftStarted = false;
	float LogDrift = -1.0f;
	/** cm past the log's placed line, along the road away from the vehicle's start */
	float MaxLogCross = 0.0f;
	FVector LogRoadDirection = FVector::ZeroVector;
	/** cm */
	float MaxAnchorDistance = 0.0f;
	FVector AnchorStart = FVector::ZeroVector;
	/** Unit, anchor to winch point at script start */
	FVector AnchorPullDirection = FVector::ZeroVector;

	float TautTime = -1.0f;
	FFloatInterval StretchFirst;
	FFloatInterval StretchLast;
};
