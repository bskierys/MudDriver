#include "B1ImGuiMud.h"
#include "B1Log.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1VehicleMovementComponent.h"
#include "NetImguiModule.h"
#include "WheeledVehiclePawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"
#include "B1ImGuiAssetSave.h"
#include "B1ImGuiWidgets.h"

namespace B1ImGui
{
#if NETIMGUI_ENABLED
	static void DrawWheelTable(const UB1VehicleMovementComponent& Movement)
	{
		constexpr ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (!ImGui::BeginTable("MudWheels", 6, Flags))
		{
			return;
		}

		ImGui::TableSetupColumn("Wheel");
		ImGui::TableSetupColumn("Material");
		ImGui::TableSetupColumn("Sink cm", ImGuiTableColumnFlags_WidthFixed, 160.0f);
		ImGui::TableSetupColumn("Slip");
		ImGui::TableSetupColumn("Friction x");
		ImGui::TableSetupColumn("Resist N");
		ImGui::TableHeadersRow();

		for (int32 WheelIndex = 0; WheelIndex < Movement.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Movement.GetWheelState(WheelIndex);
			const UB1PhysicalMaterial_Mud* Mud = Cast<UB1PhysicalMaterial_Mud>(Status.PhysMaterial.Get());
			const float Depth = Movement.GetWheelSinkDepth(WheelIndex);
			const float MaxDepth = Mud && Mud->MaxSinkDepth > 0.0f ? Mud->MaxSinkDepth : 1.0f;

			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d", WheelIndex);
			ImGui::TableNextColumn(); ImGui::Text("%s", TCHAR_TO_UTF8(*GetNameSafe(Status.PhysMaterial.Get())));
			ImGui::TableNextColumn();
			{
				char Overlay[32];
				FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.1f", Depth);
				ImGui::ProgressBar(Mud ? Depth / MaxDepth : 0.0f, ImVec2(-1.0f, 0.0f), Overlay);
			}
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Status.SlipMagnitude);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Movement.GetWheelMudFrictionScale(WheelIndex));
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Movement.GetWheelRollingResistance(WheelIndex));
		}

		ImGui::EndTable();
	}

	static bool DrawCurve(const char* Label, FRuntimeFloatCurve& Curve)
	{
		FRichCurve* RichCurve = Curve.GetRichCurve();
		bool bChanged = false;

		ImGui::PushID(Label);
		ImGui::Text("%s (depth 0..1 -> value)", Label);
		// SetKeyTime can reorder keys, so iterate over a copy of the handles.
		TArray<FKeyHandle> Handles;
		for (auto It = RichCurve->GetKeyHandleIterator(); It; ++It)
		{
			Handles.Add(*It);
		}

		for (int32 KeyIndex = 0; KeyIndex < Handles.Num(); ++KeyIndex)
		{
			FRichCurveKey& Key = RichCurve->GetKey(Handles[KeyIndex]);
			float TimeValue[2] = { Key.Time, Key.Value };
			ImGui::PushID(KeyIndex);
			ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * 0.6f);
			bool bKeyChanged = ImGui::DragFloat2("##key", TimeValue, 0.005f);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::SetNextItemWidth(TypeFieldWidth * 2.0f);
			bKeyChanged |= ImGui::InputFloat2("##type", TimeValue, "%.3f");
			if (bKeyChanged)
			{
				Key.Value = TimeValue[1];
				RichCurve->SetKeyTime(Handles[KeyIndex], FMath::Clamp(TimeValue[0], 0.0f, 1.0f));
				bChanged = true;
			}
			ImGui::PopID();
		}
		ImGui::PopID();

		if (bChanged)
		{
			RichCurve->AutoSetTangents();
		}
		return bChanged;
	}

	static void SavePreset(UB1PhysicalMaterial_Mud& Mud)
	{
		const bool bSaved = SaveAssetPackage(Mud);
		UE_LOG(LogB1Physics, Log, TEXT("Mud preset %s %s"), *Mud.GetPathName(), bSaved ? TEXT("saved") : TEXT("FAILED to save"));
	}

	/** Returns true when any value changed */
	static bool DrawPreset(UB1PhysicalMaterial_Mud& Mud)
	{
		bool bChanged = false;
		bChanged |= DragFloatTyped("Friction (zero depth)", &Mud.Friction, 0.01f, 0.0f, 2.0f);
		bChanged |= DragFloatTyped("Max sink depth cm", &Mud.MaxSinkDepth, 0.5f, 0.0f, 200.0f);
		bChanged |= DragFloatTyped("Static sink ratio", &Mud.StaticSinkRatio, 0.005f, 0.0f, 1.0f, "%.3f");
		bChanged |= DragFloatTyped("Settle rate cm/s", &Mud.SettleRate, 0.5f, 0.0f, 500.0f);
		bChanged |= DragFloatTyped("Sink rate cm/s", &Mud.SinkRate, 0.5f, 0.0f, 500.0f);
		bChanged |= DragFloatTyped("Full sink slip m/s", &Mud.FullSinkSlipSpeed, 0.05f, 0.1f, 50.0f);
		bChanged |= DragFloatTyped("Rise rate cm/m", &Mud.RiseRate, 0.05f, 0.0f, 100.0f);
		bChanged |= DragFloatTyped("Speed drag", &Mud.SpeedDragCoefficient, 0.001f, 0.0f, 1.0f, "%.3f");
		bChanged |= DragFloatTyped("Belly drag", &Mud.BellyDragCoefficient, 0.05f, 0.0f, 50.0f);
		bChanged |= DrawCurve("Friction x", Mud.FrictionVsDepth);
		bChanged |= DrawCurve("Rolling resistance", Mud.RollingResistanceVsDepth);
		return bChanged;
	}

	static void DrawPresets()
	{
		static TSet<TWeakObjectPtr<UB1PhysicalMaterial_Mud>> Edited;

		ImGui::TextDisabled("Edits apply on the next frame. Save writes the asset (editor only).");
		for (TObjectIterator<UB1PhysicalMaterial_Mud> It(RF_ClassDefaultObject | RF_ArchetypeObject); It; ++It)
		{
			UB1PhysicalMaterial_Mud& Mud = **It;
			const bool bEdited = Edited.Contains(&Mud);
			const FString Header = FString::Printf(TEXT("%s%s###%s"), *Mud.GetName(), bEdited ? TEXT(" *") : TEXT(""), *Mud.GetPathName());

			if (!ImGui::CollapsingHeader(TCHAR_TO_UTF8(*Header)))
			{
				continue;
			}

			ImGui::PushID(&Mud);
			if (DrawPreset(Mud))
			{
				Edited.Add(&Mud);
				Mud.MarkPackageDirty();
			}
#if WITH_EDITOR
			ImGui::BeginDisabled(!bEdited);
			if (ImGui::Button("Save to asset"))
			{
				SavePreset(Mud);
				Edited.Remove(&Mud);
			}
			ImGui::EndDisabled();
#endif
			ImGui::PopID();
		}
	}
#endif

	void DrawMud(UWorld* World, bool* bOpen)
	{
#if NETIMGUI_ENABLED
		if (!ImGui::Begin("Mud", bOpen))
		{
			ImGui::End();
			return;
		}

		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const AWheeledVehiclePawn* Vehicle = PC ? Cast<AWheeledVehiclePawn>(PC->GetPawn()) : nullptr;
		UB1VehicleMovementComponent* Movement = Vehicle ? Cast<UB1VehicleMovementComponent>(Vehicle->GetVehicleMovementComponent()) : nullptr;

		if (Movement)
		{
			ImGui::Text("Speed: %.1f km/h", Movement->GetForwardSpeed() * 0.036f);
			ImGui::Text("Belly: %.1f cm past clearance, drag %.0f N", Movement->GetBellyDepth(), Movement->GetBellyDrag());
			DragFloatTyped("Belly clearance cm", &Movement->BellyClearance, 0.5f, 0.0f, 200.0f);
			DrawWheelTable(*Movement);
		}
		else
		{
			ImGui::TextDisabled("No B1 vehicle");
		}

		if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawPresets();
		}

		ImGui::End();
#endif
	}
}
