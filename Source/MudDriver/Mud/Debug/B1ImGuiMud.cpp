#include "B1ImGuiMud.h"
#include "B1MudLog.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1MudVehicleComponent.h"
#include "B1MudDirtComponent.h"
#include "B1MudSprayComponent.h"
#include "B1MudLensComponent.h"
#include "NetImguiModule.h"
#include "Engine/Blueprint.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"
#include "B1ImGuiAssetSave.h"
#include "B1ImGuiWidgets.h"

namespace B1ImGui
{
#if NETIMGUI_ENABLED
	static void DrawWheelTable(const UB1MudVehicleComponent& Mud)
	{
		constexpr ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (!ImGui::BeginTable("MudWheels", 7, Flags))
		{
			return;
		}

		ImGui::TableSetupColumn("Wheel");
		ImGui::TableSetupColumn("Material");
		ImGui::TableSetupColumn("Sink cm", ImGuiTableColumnFlags_WidthFixed, 160.0f);
		ImGui::TableSetupColumn("Slip");
		ImGui::TableSetupColumn("Friction x");
		ImGui::TableSetupColumn("Resist N");
		ImGui::TableSetupColumn("Pull N");
		ImGui::TableHeadersRow();

		for (int32 WheelIndex = 0; WheelIndex < Mud.GetNumWheels(); ++WheelIndex)
		{
			const FWheelStatus& Status = Mud.GetWheelState(WheelIndex);
			const UB1PhysicalMaterial_Mud* WheelMud = Mud.GetWheelMud(WheelIndex);
			const float Depth = Mud.GetWheelSinkDepth(WheelIndex);
			const float MaxDepth = WheelMud && WheelMud->MaxSinkDepth > 0.0f ? WheelMud->MaxSinkDepth : 1.0f;

			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d", WheelIndex);
			ImGui::TableNextColumn(); ImGui::Text("%s", TCHAR_TO_UTF8(*GetNameSafe(Status.PhysMaterial.Get())));
			ImGui::TableNextColumn();
			{
				char Overlay[32];
				FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.1f", Depth);
				ImGui::ProgressBar(WheelMud ? Depth / MaxDepth : 0.0f, ImVec2(-1.0f, 0.0f), Overlay);
			}
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Status.SlipMagnitude);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Mud.GetWheelMudFrictionScale(WheelIndex));
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Mud.GetWheelRollingResistance(WheelIndex));
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Mud.GetWheelSlopePull(WheelIndex));
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

	static void DrawDirtBar(const char* Label, float Value, float Intensity)
	{
		char Overlay[48];
		if (Intensity >= 0.0f)
		{
			FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.2f  (throw %.2f)", Value, Intensity);
		}
		else
		{
			FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.2f", Value);
		}
		ImGui::ProgressBar(Value, ImVec2(-120.0f, 0.0f), Overlay);
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted(Label);
	}

#if WITH_EDITOR
	/** Copies the Mud|Dirt and Mud|Dirt|Tire tuning (not PreviewDirt) to the pawn BP's component template and saves the BP */
	static bool SaveDirtDefaults(const UB1MudDirtComponent& Dirt)
	{
		UB1MudDirtComponent* Archetype = Cast<UB1MudDirtComponent>(Dirt.GetArchetype());
		const AActor* Owner = Dirt.GetOwner();
		UBlueprint* Blueprint = Owner ? Cast<UBlueprint>(Owner->GetClass()->ClassGeneratedBy) : nullptr;
		if (!Archetype || !Blueprint || Archetype->GetPackage() != Blueprint->GetPackage())
		{
			return false;
		}

		Archetype->Modify();
		Archetype->WheelSecondsToFull = Dirt.WheelSecondsToFull;
		Archetype->BodySecondsToFull = Dirt.BodySecondsToFull;
		Archetype->BellyFullDepth = Dirt.BellyFullDepth;
		Archetype->BellyBodyRate = Dirt.BellyBodyRate;
		Archetype->TreadCoat = Dirt.TreadCoat;
		Archetype->TireCoatSecondsToFull = Dirt.TireCoatSecondsToFull;
		Archetype->TireShedDistance = Dirt.TireShedDistance;
		Archetype->TireDrySeconds = Dirt.TireDrySeconds;
		Archetype->MarkPackageDirty();
		return SaveAssetPackage(*Blueprint);
	}
#endif

	static void DrawDirt(UB1MudDirtComponent& Dirt)
	{
		static TSet<TWeakObjectPtr<UB1MudDirtComponent>> Edited;

		for (int32 WheelIndex = 0; WheelIndex < UB1MudDirtComponent::NumWheelSlots; ++WheelIndex)
		{
			char Label[16];
			FCStringAnsi::Snprintf(Label, sizeof(Label), "w%d", WheelIndex);
			DrawDirtBar(Label, Dirt.GetWheelDirt(WheelIndex), Dirt.GetWheelIntensity(WheelIndex));
		}
		DrawDirtBar("body", Dirt.GetBodyDirt(), -1.0f);
		for (int32 WheelIndex = 0; WheelIndex < UB1MudDirtComponent::NumWheelSlots; ++WheelIndex)
		{
			char Overlay[48];
			FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "coat %.2f  wet %.2f", Dirt.GetTireCoat(WheelIndex), Dirt.GetTireWetness(WheelIndex));
			ImGui::ProgressBar(Dirt.GetTireCoat(WheelIndex), ImVec2(-120.0f, 0.0f), Overlay);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::Text("tire w%d", WheelIndex);
		}
		ImGui::Text("Targets: %d tires, %d body", Dirt.GetNumTireTargets(), Dirt.GetNumBodyTargets());

		if (ImGui::Button("Clean"))
		{
			Dirt.Clean();
		}
		ImGui::SameLine();
		if (ImGui::Button("Max"))
		{
			Dirt.SetDirt(1.0f);
		}

		float Preview = Dirt.PreviewDirt;
		if (DragFloatTyped("Preview dirt (-1 off)", &Preview, 0.01f, -1.0f, 1.0f))
		{
			Dirt.SetPreviewDirt(Preview);
		}

		bool bChanged = false;
		bChanged |= DragFloatTyped("Wheel s to full", &Dirt.WheelSecondsToFull, 0.5f, 0.1f, 600.0f);
		bChanged |= DragFloatTyped("Body s to full", &Dirt.BodySecondsToFull, 0.5f, 0.1f, 600.0f);
		bChanged |= DragFloatTyped("Belly full depth cm", &Dirt.BellyFullDepth, 0.5f, 1.0f, 200.0f);
		bChanged |= DragFloatTyped("Belly body rate", &Dirt.BellyBodyRate, 0.01f, 0.0f, 10.0f);
		bChanged |= DragFloatTyped("Tread coat", &Dirt.TreadCoat, 0.01f, 0.0f, 1.0f);
		bChanged |= DragFloatTyped("Tire coat s to full", &Dirt.TireCoatSecondsToFull, 0.5f, 0.1f, 600.0f);
		bChanged |= DragFloatTyped("Tire shed m", &Dirt.TireShedDistance, 1.0f, 1.0f, 5000.0f);
		bChanged |= DragFloatTyped("Tire dry s", &Dirt.TireDrySeconds, 0.5f, 0.1f, 600.0f);
		if (bChanged)
		{
			Edited.Add(&Dirt);
		}

#if WITH_EDITOR
		ImGui::BeginDisabled(!Edited.Contains(&Dirt));
		if (ImGui::Button("Save to asset##Dirt"))
		{
			const bool bSaved = SaveDirtDefaults(Dirt);
			UE_LOG(LogB1Mud, Log, TEXT("Dirt defaults of %s %s"), *GetNameSafe(Dirt.GetOwner()->GetClass()), bSaved ? TEXT("saved") : TEXT("FAILED to save (not a BP vehicle?)"));
			if (bSaved)
			{
				Edited.Remove(&Dirt);
			}
		}
		ImGui::EndDisabled();
#endif
	}

#if WITH_EDITOR
	/** Copies the Mud|Spray tuning to the pawn BP's component template and saves the BP */
	static bool SaveSprayDefaults(const UB1MudSprayComponent& Spray)
	{
		UB1MudSprayComponent* Archetype = Cast<UB1MudSprayComponent>(Spray.GetArchetype());
		const AActor* Owner = Spray.GetOwner();
		UBlueprint* Blueprint = Owner ? Cast<UBlueprint>(Owner->GetClass()->ClassGeneratedBy) : nullptr;
		if (!Archetype || !Blueprint || Archetype->GetPackage() != Blueprint->GetPackage())
		{
			return false;
		}

		Archetype->Modify();
		Archetype->MinSlipSpeed = Spray.MinSlipSpeed;
		Archetype->MaxSpawnRate = Spray.MaxSpawnRate;
		Archetype->RollingRateFactor = Spray.RollingRateFactor;
		Archetype->SpraySpeedPerSlip = Spray.SpraySpeedPerSlip;
		Archetype->MinSpraySpeed = Spray.MinSpraySpeed;
		Archetype->MaxSpraySpeed = Spray.MaxSpraySpeed;
		Archetype->SprayElevation = Spray.SprayElevation;
		Archetype->SpraySpread = Spray.SpraySpread;
		Archetype->SpraySpeedSpread = Spray.SpraySpeedSpread;
		Archetype->SpawnBackOffset = Spray.SpawnBackOffset;
		Archetype->SpawnLift = Spray.SpawnLift;
		Archetype->MarkPackageDirty();
		return SaveAssetPackage(*Blueprint);
	}
#endif

	static void DrawSpray(UB1MudSprayComponent& Spray, int32 NumWheels)
	{
		static TSet<TWeakObjectPtr<UB1MudSprayComponent>> Edited;

		for (int32 WheelIndex = 0; WheelIndex < NumWheels; ++WheelIndex)
		{
			const float Rate = Spray.GetWheelSpawnRate(WheelIndex);
			char Overlay[48];
			FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.0f /s  (throw %.2f)", Rate, Spray.GetWheelIntensity(WheelIndex));
			ImGui::ProgressBar(Spray.MaxSpawnRate > 0.0f ? Rate / Spray.MaxSpawnRate : 0.0f, ImVec2(-120.0f, 0.0f), Overlay);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::Text("w%d", WheelIndex);
		}

		bool bChanged = false;
		bChanged |= DragFloatTyped("Min slip m/s", &Spray.MinSlipSpeed, 0.05f, 0.0f, 20.0f);
		bChanged |= DragFloatTyped("Max spawn rate /s", &Spray.MaxSpawnRate, 5.0f, 0.0f, 5000.0f);
		bChanged |= DragFloatTyped("Rolling rate factor", &Spray.RollingRateFactor, 0.01f, 0.0f, 1.0f);
		bChanged |= DragFloatTyped("Speed per slip", &Spray.SpraySpeedPerSlip, 0.01f, 0.0f, 10.0f);
		bChanged |= DragFloatTyped("Min speed cm/s", &Spray.MinSpraySpeed, 5.0f, 0.0f, 5000.0f);
		bChanged |= DragFloatTyped("Max speed cm/s", &Spray.MaxSpraySpeed, 5.0f, 0.0f, 5000.0f);
		bChanged |= DragFloatTyped("Elevation deg", &Spray.SprayElevation, 0.5f, 0.0f, 90.0f);
		bChanged |= DragFloatTyped("Spread deg", &Spray.SpraySpread, 0.5f, 0.0f, 90.0f);
		bChanged |= DragFloatTyped("Speed spread", &Spray.SpraySpeedSpread, 0.01f, 0.0f, 1.0f);
		bChanged |= DragFloatTyped("Spawn back offset cm", &Spray.SpawnBackOffset, 0.5f, -50.0f, 100.0f);
		bChanged |= DragFloatTyped("Spawn lift cm", &Spray.SpawnLift, 0.5f, 0.0f, 100.0f);
		if (bChanged)
		{
			Edited.Add(&Spray);
		}

#if WITH_EDITOR
		ImGui::BeginDisabled(!Edited.Contains(&Spray));
		if (ImGui::Button("Save to asset##Spray"))
		{
			const bool bSaved = SaveSprayDefaults(Spray);
			UE_LOG(LogB1Mud, Log, TEXT("Spray defaults of %s %s"), *GetNameSafe(Spray.GetOwner()->GetClass()), bSaved ? TEXT("saved") : TEXT("FAILED to save (not a BP vehicle?)"));
			if (bSaved)
			{
				Edited.Remove(&Spray);
			}
		}
		ImGui::EndDisabled();
#endif
	}

#if WITH_EDITOR
	/** Copies the Mud|Lens tuning (not PreviewLensDirt) to the pawn BP's component template and saves the BP */
	static bool SaveLensDefaults(const UB1MudLensComponent& Lens)
	{
		UB1MudLensComponent* Archetype = Cast<UB1MudLensComponent>(Lens.GetArchetype());
		const AActor* Owner = Lens.GetOwner();
		UBlueprint* Blueprint = Owner ? Cast<UBlueprint>(Owner->GetClass()->ClassGeneratedBy) : nullptr;
		if (!Archetype || !Blueprint || Archetype->GetPackage() != Blueprint->GetPackage())
		{
			return false;
		}

		Archetype->Modify();
		Archetype->LensMinThrowSpeed = Lens.LensMinThrowSpeed;
		Archetype->LensFullThrowSpeed = Lens.LensFullThrowSpeed;
		Archetype->LensAimFull = Lens.LensAimFull;
		Archetype->LensAimZero = Lens.LensAimZero;
		Archetype->LensFalloffDistance = Lens.LensFalloffDistance;
		Archetype->LensSplashThreshold = Lens.LensSplashThreshold;
		Archetype->LensSplashInterval = Lens.LensSplashInterval;
		Archetype->LensSecondsToFull = Lens.LensSecondsToFull;
		Archetype->LensHoldSeconds = Lens.LensHoldSeconds;
		Archetype->LensFadeSeconds = Lens.LensFadeSeconds;
		Archetype->MarkPackageDirty();
		return SaveAssetPackage(*Blueprint);
	}
#endif

	static void DrawLensSlots(const UB1MudLensComponent& Lens)
	{
		constexpr ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (!ImGui::BeginTable("LensSlots", 5, Flags))
		{
			return;
		}

		ImGui::TableSetupColumn("Slot");
		ImGui::TableSetupColumn("Strength", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("State");
		ImGui::TableSetupColumn("Offset");
		ImGui::TableSetupColumn("Variant");
		ImGui::TableHeadersRow();

		for (int32 SlotIndex = 0; SlotIndex < UB1MudLensComponent::NumSlots; ++SlotIndex)
		{
			const FB1MudLensSlot& Slot = Lens.GetSlot(SlotIndex);
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d", SlotIndex);
			ImGui::TableNextColumn();
			{
				char Overlay[16];
				FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.2f", Slot.Strength);
				ImGui::ProgressBar(Slot.Strength, ImVec2(-1.0f, 0.0f), Overlay);
			}
			ImGui::TableNextColumn();
			if (Slot.Strength <= 0.0f)
			{
				ImGui::TextDisabled("empty");
			}
			else if (Slot.SinceHit <= Lens.LensHoldSeconds)
			{
				ImGui::Text("hold %.1f s", Lens.LensHoldSeconds - Slot.SinceHit);
			}
			else
			{
				ImGui::Text("fade %.1f s", Slot.Strength * Lens.LensFadeSeconds);
			}
			ImGui::TableNextColumn(); ImGui::Text("%.2f, %.2f", Slot.Offset.X, Slot.Offset.Y);
			ImGui::TableNextColumn(); ImGui::Text("%d", Slot.Variant);
		}

		ImGui::EndTable();
	}

	static void DrawLensExposure(const UB1MudLensComponent& Lens)
	{
		constexpr ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (!ImGui::BeginTable("LensExposure", 8, Flags))
		{
			return;
		}

		ImGui::TableSetupColumn("Wheel");
		ImGui::TableSetupColumn("Exposure", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Intensity");
		ImGui::TableSetupColumn("Throw");
		ImGui::TableSetupColumn("Aim");
		ImGui::TableSetupColumn("Distance");
		ImGui::TableSetupColumn("Speed cm/s");
		ImGui::TableSetupColumn("Angle deg");
		ImGui::TableHeadersRow();

		for (int32 WheelIndex = 0; WheelIndex < Lens.GetNumWheelExposures(); ++WheelIndex)
		{
			const FB1MudLensWheelExposure& Wheel = Lens.GetWheelExposure(WheelIndex);
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d", WheelIndex);
			ImGui::TableNextColumn();
			{
				char Overlay[16];
				FCStringAnsi::Snprintf(Overlay, sizeof(Overlay), "%.2f", Wheel.Exposure);
				ImGui::ProgressBar(Wheel.Exposure, ImVec2(-1.0f, 0.0f), Overlay);
			}
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Wheel.Intensity);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Wheel.Throw);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Wheel.Aim);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", Wheel.Distance);
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Wheel.LaunchSpeed);
			ImGui::TableNextColumn(); ImGui::Text("%.0f", Wheel.AimAngle);
		}

		ImGui::EndTable();
	}

	static void DrawLens(UB1MudLensComponent& Lens)
	{
		static TSet<TWeakObjectPtr<UB1MudLensComponent>> Edited;

		DrawDirtBar("lens", Lens.GetLensDirt(), -1.0f);
		ImGui::Text("Exposure %.2f  splashes %d  cameras %d", Lens.GetExposure(), Lens.GetSplashCount(), Lens.GetNumCameras());
		DrawLensSlots(Lens);
		DrawLensExposure(Lens);

		if (ImGui::Button("Splash"))
		{
			Lens.Splash();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clean"))
		{
			Lens.CleanLens();
		}
		ImGui::SameLine();
		if (ImGui::Button("Max"))
		{
			Lens.SetLensDirt(1.0f);
		}

		float Preview = Lens.PreviewLensDirt;
		if (DragFloatTyped("Preview lens (-1 off)", &Preview, 0.01f, -1.0f, 1.0f))
		{
			Lens.SetPreviewLensDirt(Preview);
		}

		bool bChanged = false;
		bChanged |= DragFloatTyped("Min throw cm/s", &Lens.LensMinThrowSpeed, 5.0f, 0.0f, 5000.0f);
		bChanged |= DragFloatTyped("Full throw cm/s", &Lens.LensFullThrowSpeed, 5.0f, 0.0f, 5000.0f);
		bChanged |= DragFloatTyped("Aim full deg", &Lens.LensAimFull, 0.5f, 0.0f, 180.0f);
		bChanged |= DragFloatTyped("Aim zero deg", &Lens.LensAimZero, 0.5f, 0.0f, 180.0f);
		bChanged |= DragFloatTyped("Falloff distance cm", &Lens.LensFalloffDistance, 5.0f, 1.0f, 10000.0f);
		bChanged |= DragFloatTyped("Splash threshold", &Lens.LensSplashThreshold, 0.01f, 0.0f, 1.0f);
		bChanged |= DragFloatTyped("Splash interval s", &Lens.LensSplashInterval, 0.01f, 0.02f, 5.0f);
		bChanged |= DragFloatTyped("s to full", &Lens.LensSecondsToFull, 0.1f, 0.1f, 60.0f);
		bChanged |= DragFloatTyped("Hold s", &Lens.LensHoldSeconds, 0.1f, 0.0f, 30.0f);
		bChanged |= DragFloatTyped("Fade s", &Lens.LensFadeSeconds, 0.1f, 0.1f, 60.0f);
		if (bChanged)
		{
			Edited.Add(&Lens);
		}

#if WITH_EDITOR
		ImGui::BeginDisabled(!Edited.Contains(&Lens));
		if (ImGui::Button("Save to asset##Lens"))
		{
			const bool bSaved = SaveLensDefaults(Lens);
			UE_LOG(LogB1Mud, Log, TEXT("Lens defaults of %s %s"), *GetNameSafe(Lens.GetOwner()->GetClass()), bSaved ? TEXT("saved") : TEXT("FAILED to save (not a BP vehicle?)"));
			if (bSaved)
			{
				Edited.Remove(&Lens);
			}
		}
		ImGui::EndDisabled();
#endif
	}

	static void SavePreset(UB1PhysicalMaterial_Mud& Mud)
	{
		const bool bSaved = SaveAssetPackage(Mud);
		UE_LOG(LogB1Mud, Log, TEXT("Mud preset %s %s"), *Mud.GetPathName(), bSaved ? TEXT("saved") : TEXT("FAILED to save"));
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
		const APawn* Vehicle = PC ? PC->GetPawn() : nullptr;
		UB1MudVehicleComponent* Mud = Vehicle ? Vehicle->FindComponentByClass<UB1MudVehicleComponent>() : nullptr;

		if (Mud)
		{
			ImGui::Text("Speed: %.1f km/h", FVector::DotProduct(Vehicle->GetVelocity(), Vehicle->GetActorForwardVector()) * 0.036f);
			ImGui::Text("Belly: %.1f cm past clearance, drag %.0f N", Mud->GetBellyDepth(), Mud->GetBellyDrag());
			DragFloatTyped("Belly clearance cm", &Mud->BellyClearance, 0.5f, 0.0f, 200.0f);
			DrawWheelTable(*Mud);
			// Component values on the player's vehicle, live only; set the defaults on the vehicle BP.
			DragFloatTyped("Slope pull", &Mud->SlopePull, 0.01f, 0.0f, 5.0f);
			DragFloatTyped("Slope from deg", &Mud->KeepAwakeSlope, 0.1f, 0.0f, 45.0f);
		}
		else
		{
			ImGui::TextDisabled("No B1 vehicle");
		}

		if (UB1MudDirtComponent* Dirt = Vehicle ? Vehicle->FindComponentByClass<UB1MudDirtComponent>() : nullptr)
		{
			if (ImGui::CollapsingHeader("Dirt", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushID("Dirt");
				DrawDirt(*Dirt);
				ImGui::PopID();
			}
		}

		if (UB1MudSprayComponent* Spray = Vehicle ? Vehicle->FindComponentByClass<UB1MudSprayComponent>() : nullptr)
		{
			if (ImGui::CollapsingHeader("Spray", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushID("Spray");
				DrawSpray(*Spray, Mud ? Mud->GetNumWheels() : 0);
				ImGui::PopID();
			}
		}

		if (UB1MudLensComponent* Lens = Vehicle ? Vehicle->FindComponentByClass<UB1MudLensComponent>() : nullptr)
		{
			if (ImGui::CollapsingHeader("Lens", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushID("Lens");
				DrawLens(*Lens);
				ImGui::PopID();
			}
		}

		if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawPresets();
		}

		ImGui::End();
#endif
	}
}
