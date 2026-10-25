#include "MudCheatManager.h"
#include "MudDriver.h"
#include "NetImguiModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

void UMudCheatManager::NetImGuiLaunch()
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
