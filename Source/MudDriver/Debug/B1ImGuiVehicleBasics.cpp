#include "B1ImGuiVehicleBasics.h"
#include "NetImguiModule.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace B1ImGui
{
	static constexpr float CmPerSecToKmPerHour = 0.036f;

#if NETIMGUI_ENABLED
	static void DrawWheelTable(const UChaosWheeledVehicleMovementComponent& Movement)
	{
		constexpr ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (!ImGui::BeginTable("Wheels", 8, Flags))
		{
			return;
		}

		ImGui::TableSetupColumn("Wheel");
		ImGui::TableSetupColumn("Contact");
		ImGui::TableSetupColumn("Material");
		ImGui::TableSetupColumn("Slip");
		ImGui::TableSetupColumn("Skid");
		ImGui::TableSetupColumn("Susp");
		ImGui::TableSetupColumn("Drive Nm");
		ImGui::TableSetupColumn("Brake Nm");
		ImGui::TableHeadersRow();

		const ImVec4 Highlight(1.0f, 0.6f, 0.2f, 1.0f);

		for (int32 WheelIndex = 0; WheelIndex < Movement.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Movement.GetWheelState(WheelIndex);
			const FName WheelName = Movement.WheelSetups.IsValidIndex(WheelIndex) ? Movement.WheelSetups[WheelIndex].BoneName : NAME_None;

			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d %s", WheelIndex, TCHAR_TO_UTF8(*WheelName.ToString()));
			ImGui::TableNextColumn(); ImGui::Text(Status.bInContact ? "yes" : "-");
			ImGui::TableNextColumn(); ImGui::Text("%s", TCHAR_TO_UTF8(*GetNameSafe(Status.PhysMaterial.Get())));
			ImGui::TableNextColumn(); Status.bIsSlipping ? ImGui::TextColored(Highlight, "%.2f", Status.SlipMagnitude) : ImGui::Text("%.2f", Status.SlipMagnitude);
			ImGui::TableNextColumn(); Status.bIsSkidding ? ImGui::TextColored(Highlight, "%.2f", Status.SkidMagnitude) : ImGui::Text("%.2f", Status.SkidMagnitude);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Status.NormalizedSuspensionLength);
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Status.DriveTorque);
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Status.BrakeTorque);
		}

		ImGui::EndTable();
	}
#endif

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
		UChaosVehicleMovementComponent* Movement = Vehicle ? Vehicle->GetVehicleMovementComponent() : nullptr;

		if (Movement)
		{
			ImGui::Text("Vehicle: %s", TCHAR_TO_UTF8(*Vehicle->GetName()));
			ImGui::Text("Speed: %.1f km/h   Gear: %d   Throttle: %.0f%%", Movement->GetForwardSpeed() * CmPerSecToKmPerHour,
				Movement->GetCurrentGear(), Movement->GetThrottleInput() * 100.0f);

			if (const UChaosWheeledVehicleMovementComponent* WheeledMovement = Cast<UChaosWheeledVehicleMovementComponent>(Movement))
			{
				const float Rpm = WheeledMovement->GetEngineRotationSpeed();
				const float MaxRpm = WheeledMovement->GetEngineMaxRotationSpeed();
				char Overlay[48];
				FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.0f / %.0f rpm", Rpm, MaxRpm);
				ImGui::ProgressBar(MaxRpm > 0.0f ? Rpm / MaxRpm : 0.0f, ImVec2(-1.0f, 0.0f), Overlay);

				DrawWheelTable(*WheeledMovement);
				ImGui::TextDisabled("Orange = slipping / skidding");
			}
		}
		else
		{
			ImGui::TextDisabled("No player vehicle");
		}

		ImGui::End();
#endif
	}
}
