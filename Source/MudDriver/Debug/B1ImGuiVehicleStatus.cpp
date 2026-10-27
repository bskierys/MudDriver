#include "B1ImGuiVehicleStatus.h"
#include "B1AbilitySystemComponent.h"
#include "B1ImGuiAssetSave.h"
#include "B1ImGuiWidgets.h"
#include "B1Log.h"
#include "B1FuelConsumptionComponent.h"
#include "B1FuelConsumptionData.h"
#include "B1MudVehicleComponent.h"
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

	static void DrawFuelUse(const UB1FuelConsumptionComponent& Fuel, UB1MudVehicleComponent* Mud)
	{
		const float Per100m = Fuel.GetFuelPer100m();
		ImGui::Text("Engine power %3.0f %%", Fuel.GetEnginePower() * 100.0f);
		for (const FB1FuelLoad& Load : Fuel.GetLoads())
		{
			ImGui::Text("%s load %3.0f %%", TCHAR_TO_UTF8(*Load.Name.ToString()), Load.Load * 100.0f);
		}
		if (Mud)
		{
			ImGui::Text("Mud sink %.1f cm avg", Mud->GetAverageSinkDepth());
			DragFloatTyped("Mud full strain cm (live)", &Mud->MudFullStrainDepth, 0.5f, 1.0f, 200.0f, "%.1f");
		}
		ImGui::Text("Rate %.1f L/h", Fuel.GetFuelRate());
		if (Per100m >= 0.0f)
		{
			ImGui::Text("Last 5 s %.2f L/100 m", Per100m);
		}
		else
		{
			ImGui::TextDisabled("Last 5 s: not moving");
		}

		UB1FuelConsumptionData* Tuning = Fuel.GetData();
		if (!Tuning)
		{
			ImGui::TextDisabled("No consumption Data on %s", TCHAR_TO_UTF8(*GetNameSafe(Fuel.GetOwner())));
			return;
		}

		static TSet<TWeakObjectPtr<UB1FuelConsumptionData>> Edited;
		bool bChanged = false;
		bChanged |= DragFloatTyped("Idle L/h", &Tuning->IdleRate, 0.1f, 0.0f, 1000.0f, "%.1f");
		bChanged |= DragFloatTyped("Work L/h", &Tuning->WorkRate, 1.0f, 0.0f, 5000.0f, "%.0f");
		for (TPair<FName, float>& Rate : Tuning->LoadRates)
		{
			const FString Label = Rate.Key.ToString() + TEXT(" work L/h");
			bChanged |= DragFloatTyped(TCHAR_TO_UTF8(*Label), &Rate.Value, 1.0f, 0.0f, 5000.0f, "%.0f");
		}
		bChanged |= DragFloatTyped("Drain interval s", &Tuning->DrainInterval, 0.01f, 0.02f, 2.0f, "%.2f");
		if (bChanged)
		{
			Edited.Add(Tuning);
			Tuning->MarkPackageDirty();
		}
#if WITH_EDITOR
		ImGui::BeginDisabled(!Edited.Contains(Tuning));
		if (ImGui::Button("Save to asset"))
		{
			const bool bSaved = SaveAssetPackage(*Tuning);
			UE_LOG(LogB1Vehicle, Log, TEXT("Tuning %s %s"), *Tuning->GetPathName(), bSaved ? TEXT("saved") : TEXT("FAILED to save"));
			Edited.Remove(Tuning);
		}
		ImGui::EndDisabled();
#endif
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

		const AActor* Avatar = ASC->GetAvatarActor();
		const UB1FuelConsumptionComponent* Fuel = Avatar ? Avatar->FindComponentByClass<UB1FuelConsumptionComponent>() : nullptr;
		if (Fuel && ImGui::CollapsingHeader("Fuel use", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawFuelUse(*Fuel, Avatar->FindComponentByClass<UB1MudVehicleComponent>());
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
