#include "B1ImGuiWinch.h"
#include "B1ImGuiAssetSave.h"
#include "B1ImGuiWidgets.h"
#include "B1Log.h"
#include "B1MudVehicleComponent.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "NetImguiModule.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace B1ImGui
{
#if NETIMGUI_ENABLED
	static void DrawBar(const char* Label, float Value, float Max)
	{
		char Overlay[48];
		FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.1f / %.1f", Value, Max);
		ImGui::ProgressBar(Max > 0.0f ? Value / Max : 0.0f, ImVec2(-120.0f, 0.0f), Overlay);
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted(Label);
	}

	static void DrawPushTable(const UB1MudVehicleComponent& Mud)
	{
		constexpr ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (!ImGui::BeginTable("WinchPush", 5, Flags))
		{
			return;
		}

		ImGui::TableSetupColumn("Wheel");
		ImGui::TableSetupColumn("Mud");
		ImGui::TableSetupColumn("Net push N");
		ImGui::TableSetupColumn("Drag N");
		ImGui::TableSetupColumn("Slope pull N");
		ImGui::TableHeadersRow();

		for (int32 WheelIndex = 0; WheelIndex < Mud.GetNumWheels(); ++WheelIndex)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d", WheelIndex);
			ImGui::TableNextColumn(); ImGui::Text("%s", TCHAR_TO_UTF8(*GetNameSafe(Mud.GetWheelMud(WheelIndex))));
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Mud.GetWheelNetPush(WheelIndex));
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Mud.GetWheelRollingResistance(WheelIndex));
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Mud.GetWheelSlopePull(WheelIndex));
		}

		ImGui::EndTable();
		ImGui::Text("Belly drag %.0f N (%.1f cm past clearance)", Mud.GetBellyDrag(), Mud.GetBellyDepth());
	}

#if WITH_EDITOR
	/** Copies Winch|Config to the vehicle BP's component template and saves the BP */
	static bool SaveWinchDefaults(const UB1WinchComponent& Winch)
	{
		UB1WinchComponent* Archetype = Cast<UB1WinchComponent>(Winch.GetArchetype());
		const AActor* Owner = Winch.GetOwner();
		UBlueprint* Blueprint = Owner ? Cast<UBlueprint>(Owner->GetClass()->ClassGeneratedBy) : nullptr;
		if (!Archetype || !Blueprint || Archetype->GetPackage() != Blueprint->GetPackage())
		{
			return false;
		}

		Archetype->Modify();
		Archetype->MaxCableLength = Winch.MaxCableLength;
		Archetype->MaxPullForce = Winch.MaxPullForce;
		Archetype->ReelInSpeed = Winch.ReelInSpeed;
		Archetype->MinRestLength = Winch.MinRestLength;
		Archetype->CableStiffness = Winch.CableStiffness;
		Archetype->CableDamping = Winch.CableDamping;
		Archetype->MarkPackageDirty();
		return SaveAssetPackage(*Blueprint);
	}
#endif

	static void DrawParams(UB1WinchComponent& Winch)
	{
		static TSet<TWeakObjectPtr<UB1WinchComponent>> Edited;
		bool bChanged = false;
		bChanged |= DragFloatTyped("Max cable cm", &Winch.MaxCableLength, 10.0f, 100.0f, 20000.0f, "%.0f");
		bChanged |= DragFloatTyped("Max pull N", &Winch.MaxPullForce, 100.0f, 0.0f, 200000.0f, "%.0f");
		bChanged |= DragFloatTyped("Reel-in cm/s", &Winch.ReelInSpeed, 1.0f, 0.0f, 1000.0f, "%.0f");
		bChanged |= DragFloatTyped("Min rest cm", &Winch.MinRestLength, 1.0f, 0.0f, 2000.0f, "%.0f");
		bChanged |= DragFloatTyped("Stiffness N/m", &Winch.CableStiffness, 1000.0f, 0.0f, 2000000.0f, "%.0f");
		bChanged |= DragFloatTyped("Damping N*s/m", &Winch.CableDamping, 100.0f, 0.0f, 500000.0f, "%.0f");
		if (bChanged)
		{
			Edited.Add(&Winch);
		}
#if WITH_EDITOR
		ImGui::BeginDisabled(!Edited.Contains(&Winch));
		if (ImGui::Button("Save to asset"))
		{
			const bool bSaved = SaveWinchDefaults(Winch);
			UE_LOG(LogB1Physics, Log, TEXT("Winch defaults of %s %s"), *GetNameSafe(Winch.GetOwner()->GetClass()), bSaved ? TEXT("saved") : TEXT("FAILED to save (not a BP vehicle?)"));
			Edited.Remove(&Winch);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("live on this vehicle; Save writes the vehicle BP");
#endif
	}
#endif

	void DrawWinch(UWorld* World, bool* bOpen)
	{
#if NETIMGUI_ENABLED
		if (!ImGui::Begin("Winch", bOpen))
		{
			ImGui::End();
			return;
		}

		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Vehicle = PC ? PC->GetPawn() : nullptr;
		UB1WinchComponent* Winch = Vehicle ? Vehicle->FindComponentByClass<UB1WinchComponent>() : nullptr;
		if (!Winch)
		{
			ImGui::TextDisabled("No winch on the player's pawn");
			ImGui::End();
			return;
		}

		if (const UB1WinchAnchorComponent* Anchor = Winch->GetAnchor())
		{
			ImGui::Text("Anchor: %s", TCHAR_TO_UTF8(*GetNameSafe(Anchor->GetOwner())));
			if (ImGui::Button("Detach"))
			{
				Winch->Detach();
			}
		}
		else
		{
			const UB1WinchAnchorComponent* Nearest = Winch->FindNearestAnchor();
			if (Nearest)
			{
				ImGui::Text("Not attached; nearest %s at %.1f m", TCHAR_TO_UTF8(*GetNameSafe(Nearest->GetOwner())),
					FVector::Dist(Winch->GetComponentLocation(), Nearest->GetComponentLocation()) / 100.0f);
			}
			else
			{
				ImGui::TextDisabled("Not attached; no anchor within %.1f m", Winch->MaxCableLength / 100.0f);
			}
			ImGui::BeginDisabled(!Nearest);
			if (ImGui::Button("Attach nearest"))
			{
				Winch->ToggleAttachNearest();
			}
			ImGui::EndDisabled();
		}

		DrawBar("Length m", Winch->GetCurrentLength() / 100.0f, Winch->MaxCableLength / 100.0f);
		ImGui::Text("Rest %.2f m", Winch->GetRestLength() / 100.0f);
		DrawBar("Tension kN", Winch->GetTension() / 1000.0f, Winch->MaxPullForce / 1000.0f);
		ImGui::Text("Pull %3.0f %%", Winch->Pull * 100.0f);

		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Vehicle);
		const bool bTag = ASC && Winch->WinchingTag.IsValid() && ASC->HasMatchingGameplayTag(Winch->WinchingTag);
		ImGui::Text("%s: %s", TCHAR_TO_UTF8(*Winch->WinchingTag.ToString()), bTag ? "on" : "off");

		if (const UB1MudVehicleComponent* Mud = Vehicle->FindComponentByClass<UB1MudVehicleComponent>();
			Mud && ImGui::CollapsingHeader("Push vs drag", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawPushTable(*Mud);
		}

		if (ImGui::CollapsingHeader("Params", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawParams(*Winch);
		}

		ImGui::End();
#endif
	}
}
