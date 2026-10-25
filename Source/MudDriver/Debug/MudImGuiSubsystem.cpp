#include "MudImGuiSubsystem.h"
#include "MudImGuiVehicleBasics.h"
#include "NetImguiModule.h"
#include "Engine/GameInstance.h"

bool UMudImGuiSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return NETIMGUI_ENABLED && Super::ShouldCreateSubsystem(Outer);
}

void UMudImGuiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if NETIMGUI_ENABLED
	RegisterWindow("Debug", "Vehicle Basics", &MudImGui::DrawVehicleBasics);

	DrawHandle = FNetImguiModule::Get().OnDrawImgui.AddUObject(this, &UMudImGuiSubsystem::DrawImGui);
#endif
}

void UMudImGuiSubsystem::Deinitialize()
{
#if NETIMGUI_ENABLED
	if (FNetImguiModule::IsAvailable())
	{
		FNetImguiModule::Get().OnDrawImgui.Remove(DrawHandle);
	}
#endif
	Windows.Reset();

	Super::Deinitialize();
}

void UMudImGuiSubsystem::RegisterWindow(const char* Menu, const char* Name, TFunction<void(UWorld*, bool*)> Draw)
{
	Windows.Add({ Menu, Name, MoveTemp(Draw) });
}

void UMudImGuiSubsystem::DrawImGui()
{
#if NETIMGUI_ENABLED
	DrawMainMenuBar();

	UWorld* World = GetGameInstance()->GetWorld();
	for (FWindowEntry& Window : Windows)
	{
		if (Window.bOpen)
		{
			Window.Draw(World, &Window.bOpen);
		}
	}
#endif
}

void UMudImGuiSubsystem::DrawMainMenuBar()
{
#if NETIMGUI_ENABLED
	if (!ImGui::BeginMainMenuBar())
	{
		return;
	}

	for (FWindowEntry& Window : Windows)
	{
		if (ImGui::BeginMenu(Window.Menu))
		{
			ImGui::MenuItem(Window.Name, nullptr, &Window.bOpen);
			ImGui::EndMenu();
		}
	}

	ImGui::EndMainMenuBar();
#endif
}
