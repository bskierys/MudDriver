#include "B1CheatManager.h"
#include "B1Jeep.h"
#include "B1Log.h"
#include "B1VehicleMovementComponent.h"
#include "B1PlayerState.h"
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

	Jeep->GetB1VehicleMovement()->Unstuck();
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
