#include "B1ImGuiVehicleStatus.h"
#include "B1AbilitySystemComponent.h"
#include "B1PlayerState.h"
#include "B1VehicleAttributeSet.h"
#include "NetImguiModule.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace B1ImGui
{
#if NETIMGUI_ENABLED
	static void DrawBar(const char* Label, float Value, float Max, const char* Unit)
	{
		char Overlay[48];
		FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.1f / %.0f%s", Value, Max, Unit);
		ImGui::ProgressBar(Max > 0.0f ? Value / Max : 0.0f, ImVec2(-120.0f, 0.0f), Overlay);
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted(Label);
	}
#endif

	void DrawVehicleStatus(UWorld* World, bool* bOpen)
	{
#if NETIMGUI_ENABLED
		if (!ImGui::Begin("Vehicle Status", bOpen))
		{
			ImGui::End();
			return;
		}

		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const AB1PlayerState* PlayerState = PC ? PC->GetPlayerState<AB1PlayerState>() : nullptr;
		UB1AbilitySystemComponent* ASC = PlayerState ? PlayerState->GetB1AbilitySystemComponent() : nullptr;
		const UB1VehicleAttributeSet* Set = PlayerState ? PlayerState->GetVehicleAttributeSet() : nullptr;

		if (!ASC || !Set)
		{
			ImGui::TextDisabled("No B1 PlayerState ASC (game mode isn't AB1GameMode?)");
			ImGui::End();
			return;
		}

		ImGui::Text("Avatar: %s", TCHAR_TO_UTF8(*GetNameSafe(ASC->GetAvatarActor())));
		DrawBar("Condition", Set->GetCondition(), Set->GetMaxCondition(), "");
		DrawBar("Fuel", Set->GetFuel(), Set->GetMaxFuel(), " L");

		if (ImGui::Button("Refill"))
		{
			ASC->AddVehicleFuel(Set->GetMaxFuel() - Set->GetFuel());
		}
		ImGui::SameLine();
		if (ImGui::Button("+10 L"))
		{
			ASC->AddVehicleFuel(10.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("-10 L"))
		{
			ASC->AddVehicleFuel(-10.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Empty"))
		{
			ASC->AddVehicleFuel(-Set->GetFuel());
		}

		if (ImGui::Button("Damage 10"))
		{
			ASC->ApplyVehicleDamage(10.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Repair"))
		{
			ASC->SetVehicleCondition(Set->GetMaxCondition());
		}

		if (ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen))
		{
			FGameplayTagContainer Tags;
			ASC->GetOwnedGameplayTags(Tags);
			if (Tags.IsEmpty())
			{
				ImGui::TextDisabled("none");
			}
			for (const FGameplayTag& Tag : Tags)
			{
				ImGui::BulletText("%s", TCHAR_TO_UTF8(*Tag.ToString()));
			}
		}

		ImGui::End();
#endif
	}
}
