#include "MudImGuiVehicleBasics.h"
#include "NetImguiModule.h"
#include "WheeledVehiclePawn.h"
#include "ChaosVehicleMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace MudImGui
{
	static constexpr float CmPerSecToKmPerHour = 0.036f;

	void DrawVehicleBasics(UWorld* World, bool* bOpen)
	{
#if NETIMGUI_ENABLED
		if (!ImGui::Begin("Vehicle Basics", bOpen))
		{
			ImGui::End();
			return;
		}

		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const AWheeledVehiclePawn* Vehicle = PC ? Cast<AWheeledVehiclePawn>(PC->GetPawn()) : nullptr;
		const UChaosVehicleMovementComponent* Movement = Vehicle ? Vehicle->GetVehicleMovementComponent() : nullptr;

		if (Movement)
		{
			ImGui::Text("Vehicle: %s", TCHAR_TO_UTF8(*Vehicle->GetName()));
			ImGui::Text("Speed: %.1f km/h", Movement->GetForwardSpeed() * CmPerSecToKmPerHour);
		}
		else
		{
			ImGui::TextDisabled("No player vehicle");
		}

		ImGui::End();
#endif
	}
}
