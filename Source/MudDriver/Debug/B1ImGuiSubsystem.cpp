#include "B1ImGuiSubsystem.h"
#include "B1ImGuiVehicleBasics.h"
#include "B1ImGuiMud.h"
#include "B1ImGuiVehicleStatus.h"
#include "NetImguiModule.h"
#include "Engine/GameInstance.h"

bool UB1ImGuiSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return NETIMGUI_ENABLED && Super::ShouldCreateSubsystem(Outer);
}

void UB1ImGuiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if NETIMGUI_ENABLED
	RegisterWindow("Debug", "Vehicle Basics", &B1ImGui::DrawVehicleBasics);
	RegisterWindow("Debug", "Mud", &B1ImGui::DrawMud);
	RegisterWindow("Debug", "Vehicle Status", &B1ImGui::DrawVehicleStatus);

	DrawHandle = FNetImguiModule::Get().OnDrawImgui.AddUObject(this, &UB1ImGuiSubsystem::DrawImGui);
#endif
}

void UB1ImGuiSubsystem::Deinitialize()
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

void UB1ImGuiSubsystem::RegisterWindow(const char* Menu, const char* Name, TFunction<void(UWorld*, bool*)> Draw)
{
	Windows.Add({ Menu, Name, MoveTemp(Draw) });
}

void UB1ImGuiSubsystem::DrawImGui()
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

void UB1ImGuiSubsystem::DrawMainMenuBar()
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
