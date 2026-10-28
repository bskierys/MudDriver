#include "B1CheatManager.h"
#include "B1Jeep.h"
#include "B1Log.h"
#include "B1VehicleMovementComponent.h"
#include "B1MudVehicleComponent.h"
#include "B1WinchComponent.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchLog.h"
#include "B1PlayerState.h"
#include "B1Character.h"
#include "B1GameMode.h"
#include "B1PlayerController.h"
#include "EngineUtils.h"
#include "B1AbilitySystemComponent.h"
#include "B1VehicleAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "MudDriver.h"
#include "NetImguiModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

void UB1CheatManager::NetImGuiLaunch()
{
#if NETIMGUI_ENABLED && PLATFORM_WINDOWS
	if (FNetImguiModule::Get().IsConnected())
	{
		UE_LOG(LogMudDriver, Display, TEXT("NetImGuiLaunch: already connected to NetImguiServer"));
		return;
	}

	static const TCHAR* ServerExeName = TEXT("netImguiServer.exe");
	if (FPlatformProcess::IsApplicationRunning(ServerExeName))
	{
		UE_LOG(LogMudDriver, Display, TEXT("NetImGuiLaunch: NetImguiServer already running, waiting for it to connect"));
		return;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NetImgui"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogMudDriver, Error, TEXT("NetImGuiLaunch: NetImgui plugin not found"));
		return;
	}

	// Server reads netImgui.cfg (client list with localhost game/editor ports) from its working dir.
	const FString ServerDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("NetImguiServer")));
	const FString ServerExe = FPaths::Combine(ServerDir, ServerExeName);

	FProcHandle Proc = FPlatformProcess::CreateProc(*ServerExe, nullptr, true, false, false, nullptr, 0, *ServerDir, nullptr);
	if (!Proc.IsValid())
	{
		UE_LOG(LogMudDriver, Error, TEXT("NetImGuiLaunch: failed to start %s"), *ServerExe);
		return;
	}
	FPlatformProcess::CloseProc(Proc);

	UE_LOG(LogMudDriver, Display, TEXT("NetImGuiLaunch: started %s"), *ServerExe);
#else
	UE_LOG(LogMudDriver, Warning, TEXT("NetImGuiLaunch: NetImgui is not available in this build"));
#endif
}

void UB1CheatManager::B1TestForce(const FString& Mode)
{
	static const TMap<FString, EB1TestForce> Modes = {
		{ TEXT("off"), EB1TestForce::Off },
		{ TEXT("push"), EB1TestForce::Push },
		{ TEXT("spring"), EB1TestForce::Spring },
	};

	const EB1TestForce* NewMode = Modes.Find(Mode.ToLower());
	if (!NewMode)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("B1TestForce: unknown mode '%s', use off | push | spring"), *Mode);
		return;
	}

	const AB1Jeep* Jeep = Cast<AB1Jeep>(GetOuterAPlayerController()->GetPawn());
	if (!Jeep)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("B1TestForce: the player isn't driving an AB1Jeep"));
		return;
	}

	Jeep->GetB1VehicleMovement()->SetTestForce(*NewMode);
}

void UB1CheatManager::B1Unstuck()
{
	const AB1Jeep* Jeep = Cast<AB1Jeep>(GetOuterAPlayerController()->GetPawn());
	if (!Jeep)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("B1Unstuck: the player isn't driving an AB1Jeep"));
		return;
	}

	Jeep->GetMudVehicle()->Unstuck();
}

UB1WinchComponent* UB1CheatManager::GetPlayerWinch(const TCHAR* CheatName) const
{
	const AB1Jeep* Jeep = Cast<AB1Jeep>(GetOuterAPlayerController()->GetPawn());
	if (!Jeep)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("%s: the player isn't driving an AB1Jeep"), CheatName);
		return nullptr;
	}
	return Jeep->GetWinch();
}

void UB1CheatManager::B1WinchAttachNearest()
{
	UB1WinchComponent* Winch = GetPlayerWinch(TEXT("B1WinchAttachNearest"));
	if (!Winch)
	{
		return;
	}

	UB1WinchAnchorComponent* Anchor = Winch->FindNearestAnchor();
	if (!Anchor)
	{
		UE_LOG(LogB1Physics, Warning, TEXT("B1WinchAttachNearest: no anchor within %.1f m"), Winch->MaxCableLength / 100.0f);
		return;
	}

	Winch->AttachTo(Anchor);
}

void UB1CheatManager::B1WinchDetach()
{
	if (UB1WinchComponent* Winch = GetPlayerWinch(TEXT("B1WinchDetach")))
	{
		Winch->Detach();
	}
}

void UB1CheatManager::B1LogReset()
{
	int32 NumLogs = 0;
	for (AB1WinchLog* Log : TActorRange<AB1WinchLog>(GetWorld()))
	{
		Log->ResetToPlacedPose();
		++NumLogs;
	}
	UE_LOG(LogB1Physics, Log, TEXT("B1LogReset: %d log(s) back to their placed pose"), NumLogs);
}

void UB1CheatManager::B1SpawnCharacter()
{
	APlayerController* PC = GetOuterAPlayerController();
	const APawn* CurrentPawn = PC->GetPawn();
	const AB1GameMode* GameMode = GetWorld()->GetAuthGameMode<AB1GameMode>();
	if (!GameMode || !GameMode->CharacterClass)
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("B1SpawnCharacter: game mode isn't AB1GameMode or has no CharacterClass"));
		return;
	}
	if (!CurrentPawn || CurrentPawn->IsA<AB1Character>())
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("B1SpawnCharacter: needs a pawn that isn't already the character"));
		return;
	}

	constexpr float SpawnAheadCm = 500.0f;
	constexpr float SpawnUpCm = 100.0f;
	const FRotator Yaw(0.0f, CurrentPawn->GetActorRotation().Yaw, 0.0f);
	const FVector Location = CurrentPawn->GetActorLocation() + Yaw.Vector() * SpawnAheadCm + FVector(0.0f, 0.0f, SpawnUpCm);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AB1Character* Character = GetWorld()->SpawnActor<AB1Character>(GameMode->CharacterClass, Location, Yaw, Params);
	if (!Character)
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("B1SpawnCharacter: spawn failed"));
		return;
	}

	PC->Possess(Character);
	PC->SetControlRotation(Yaw);
	UE_LOG(LogB1Vehicle, Display, TEXT("B1SpawnCharacter: %s at %s"), *Character->GetName(), *Location.ToCompactString());
}

void UB1CheatManager::B1ExitVehicle()
{
	if (AB1PlayerController* PC = Cast<AB1PlayerController>(GetOuterAPlayerController()))
	{
		PC->ExitVehicle();
	}
}

void UB1CheatManager::B1EnterVehicle()
{
	AB1PlayerController* PC = Cast<AB1PlayerController>(GetOuterAPlayerController());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	AB1Jeep* Nearest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();
	for (TActorIterator<AB1Jeep> It(GetWorld()); It; ++It)
	{
		const double DistSq = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (!It->GetController() && DistSq < NearestDistSq)
		{
			Nearest = *It;
			NearestDistSq = DistSq;
		}
	}
	PC->EnterVehicle(Nearest);
}

UB1AbilitySystemComponent* UB1CheatManager::GetPlayerASC(const TCHAR* CheatName) const
{
	const AB1PlayerState* PlayerState = GetOuterAPlayerController()->GetPlayerState<AB1PlayerState>();
	UB1AbilitySystemComponent* ASC = PlayerState ? PlayerState->GetB1AbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		UE_LOG(LogB1Vehicle, Warning, TEXT("%s: no B1 PlayerState ASC (game mode isn't AB1GameMode?)"), CheatName);
	}
	return ASC;
}

void UB1CheatManager::B1DamageVehicle(float Amount)
{
	UB1AbilitySystemComponent* ASC = GetPlayerASC(TEXT("B1DamageVehicle"));
	if (!ASC)
	{
		return;
	}

	const float Before = ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute());
	const bool bApplied = ASC->ApplyVehicleDamage(Amount);
	UE_LOG(LogB1Vehicle, Display, TEXT("B1DamageVehicle %.1f: Condition %.1f -> %.1f%s"), Amount, Before,
		ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute()), bApplied ? TEXT("") : TEXT(" (DamageEffect not set)"));
}

void UB1CheatManager::B1SetCondition(float Value)
{
	UB1AbilitySystemComponent* ASC = GetPlayerASC(TEXT("B1SetCondition"));
	if (!ASC)
	{
		return;
	}

	const float Before = ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute());
	const bool bApplied = ASC->SetVehicleCondition(Value);
	UE_LOG(LogB1Vehicle, Display, TEXT("B1SetCondition %.1f: Condition %.1f -> %.1f%s"), Value, Before,
		ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetConditionAttribute()), bApplied ? TEXT("") : TEXT(" (DamageEffect not set)"));
}

void UB1CheatManager::B1AddFuel(float Litres)
{
	UB1AbilitySystemComponent* ASC = GetPlayerASC(TEXT("B1AddFuel"));
	if (!ASC)
	{
		return;
	}

	const float Before = ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute());
	const bool bApplied = ASC->AddVehicleFuel(Litres);
	UE_LOG(LogB1Vehicle, Display, TEXT("B1AddFuel %.1f: Fuel %.1f -> %.1f%s"), Litres, Before,
		ASC->GetNumericAttribute(UB1VehicleAttributeSet::GetFuelAttribute()), bApplied ? TEXT("") : TEXT(" (fuel effect not set)"));
}
