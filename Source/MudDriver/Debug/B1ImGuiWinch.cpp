#include "B1ImGuiWinch.h"
#include "B1ImGuiAssetSave.h"
#include "B1ImGuiWidgets.h"
#include "B1Log.h"
#include "B1MudVehicleComponent.h"
#include "B1PhysicalMaterial_Mud.h"
#include "B1WinchAnchorComponent.h"
#include "B1WinchComponent.h"
#include "B1WinchLog.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
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
		Archetype->ReelOutSpeed = Winch.ReelOutSpeed;
		Archetype->MinRestLength = Winch.MinRestLength;
		Archetype->CableStiffness = Winch.CableStiffness;
		Archetype->CableDamping = Winch.CableDamping;
		Archetype->HandbrakeHoldForce = Winch.HandbrakeHoldForce;
		Archetype->HandbrakeHoldSpeed = Winch.HandbrakeHoldSpeed;
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
		bChanged |= DragFloatTyped("Reel-out cm/s", &Winch.ReelOutSpeed, 1.0f, 0.0f, 1000.0f, "%.0f");
		bChanged |= DragFloatTyped("Min rest cm", &Winch.MinRestLength, 1.0f, 0.0f, 2000.0f, "%.0f");
		bChanged |= DragFloatTyped("Stiffness N/m", &Winch.CableStiffness, 1000.0f, 0.0f, 2000000.0f, "%.0f");
		bChanged |= DragFloatTyped("Damping N*s/m", &Winch.CableDamping, 100.0f, 0.0f, 500000.0f, "%.0f");
		bChanged |= DragFloatTyped("Hold N", &Winch.HandbrakeHoldForce, 100.0f, 0.0f, 300000.0f, "%.0f");
		bChanged |= DragFloatTyped("Hold below cm/s", &Winch.HandbrakeHoldSpeed, 1.0f, 0.0f, 500.0f, "%.0f");
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

#if NETIMGUI_ENABLED
#if WITH_EDITOR
	/** Copies Log|Config to the log BP's CDO and saves the BP */
	static bool SaveLogDefaults(const AB1WinchLog& Log)
	{
		AB1WinchLog* Defaults = Cast<AB1WinchLog>(Log.GetClass()->GetDefaultObject());
		UBlueprint* Blueprint = Cast<UBlueprint>(Log.GetClass()->ClassGeneratedBy);
		if (!Defaults || !Blueprint)
		{
			return false;
		}

		Defaults->Modify();
		Defaults->TrunkMass = Log.TrunkMass;
		Defaults->RootBallMass = Log.RootBallMass;
		Defaults->BreakawayTension = Log.BreakawayTension;
		Defaults->RefreezeDelay = Log.RefreezeDelay;
		Defaults->MarkPackageDirty();
		return SaveAssetPackage(*Blueprint);
	}
#endif

	/** Nearest fallen log to the player: state, config and reset */
	static void DrawLogSection(UWorld* World, const APawn* PlayerPawn)
	{
		AB1WinchLog* Log = nullptr;
		double NearestDistSq = TNumericLimits<double>::Max();
		for (AB1WinchLog* Candidate : TActorRange<AB1WinchLog>(World))
		{
			const double DistSq = PlayerPawn ? FVector::DistSquared(PlayerPawn->GetActorLocation(), Candidate->GetActorLocation()) : 0.0;
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				Log = Candidate;
			}
		}
		if (!Log || !ImGui::CollapsingHeader("Log", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const bool bFree = Log->GetState() == EB1LogState::Free;
		ImGui::Text("%s: %s, %.1f m away", TCHAR_TO_UTF8(*Log->GetActorNameOrLabel()), bFree ? "FREE" : "frozen", FMath::Sqrt(NearestDistSq) / 100.0f);
		DrawBar("Tension / breakaway kN", Log->GetTension() / 1000.0f, Log->BreakawayTension / 1000.0f);
		ImGui::Text("Switches %d  yaw %+.1f deg  mass %.0f kg", Log->GetSwitchCount(), Log->GetYawSincePlaced(), Log->GetMass());
		if (ImGui::Button("Reset log"))
		{
			Log->ResetToPlacedPose();
		}

		static TSet<TWeakObjectPtr<AB1WinchLog>> Edited;
		bool bMassChanged = false;
		bMassChanged |= DragFloatTyped("Trunk kg", &Log->TrunkMass, 10.0f, 1.0f, 20000.0f, "%.0f");
		bMassChanged |= DragFloatTyped("Root ball kg", &Log->RootBallMass, 10.0f, 1.0f, 20000.0f, "%.0f");
		bool bChanged = bMassChanged;
		bChanged |= DragFloatTyped("Breakaway N", &Log->BreakawayTension, 100.0f, 0.0f, 200000.0f, "%.0f");
		bChanged |= DragFloatTyped("Refreeze s", &Log->RefreezeDelay, 0.05f, 0.0f, 10.0f, "%.2f");
		if (bMassChanged)
		{
			Log->ApplyMass();
		}
		if (bChanged)
		{
			Edited.Add(Log);
		}
#if WITH_EDITOR
		ImGui::BeginDisabled(!Edited.Contains(Log));
		if (ImGui::Button("Save log to asset"))
		{
			const bool bSaved = SaveLogDefaults(*Log);
			UE_LOG(LogB1Physics, Log, TEXT("Log defaults of %s %s"), *GetNameSafe(Log->GetClass()), bSaved ? TEXT("saved") : TEXT("FAILED to save (not a BP log?)"));
			Edited.Remove(Log);
		}
		ImGui::EndDisabled();
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
		const APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
		UB1WinchComponent* Winch = PlayerPawn ? PlayerPawn->FindComponentByClass<UB1WinchComponent>() : nullptr;
		if (!Winch)
		{
			Winch = UB1WinchComponent::FindCarriedBy(PlayerPawn);
		}
		const APawn* Vehicle = Winch ? Cast<APawn>(Winch->GetOwner()) : nullptr;
		if (!Winch || !Vehicle)
		{
			ImGui::TextDisabled("No winch on the player's pawn or in their hand");
			DrawLogSection(World, PlayerPawn);
			ImGui::End();
			return;
		}

		const EB1WinchHook Hook = Winch->GetHookState();
		ImGui::Text("Hook: %s", TCHAR_TO_UTF8(*StaticEnum<EB1WinchHook>()->GetNameStringByValue(static_cast<int64>(Hook))));
		if (Hook == EB1WinchHook::Carried)
		{
			ImGui::SameLine();
			ImGui::Text("by %s, winch on %s", TCHAR_TO_UTF8(*GetNameSafe(Winch->GetHookCarrier())), TCHAR_TO_UTF8(*GetNameSafe(Vehicle)));
			DrawBar("Carry m", Winch->GetCurrentLength() / 100.0f, Winch->MaxCableLength / 100.0f);
			if (const UB1WinchAnchorComponent* Aim = Winch->GetAimAnchor())
			{
				const FVector AimPoint = Aim->GetAreaPoint(Winch->GetAimAlpha());
				ImGui::Text("Aim: %s  alpha %.2f  height %.2f m  %.2f m away", TCHAR_TO_UTF8(*GetNameSafe(Aim->GetOwner())), Winch->GetAimAlpha(),
					(AimPoint.Z - Aim->GetOwner()->GetActorLocation().Z) / 100.0f,
					PlayerPawn ? FVector::Dist(PlayerPawn->GetActorLocation(), AimPoint) / 100.0f : 0.0f);
			}
			else
			{
				ImGui::TextDisabled("Aim: no anchor in reach");
			}
		}

		if (const UB1WinchAnchorComponent* Anchor = Winch->GetAnchor())
		{
			ImGui::Text("Anchor: %s  alpha %.2f  (%.0f cm along the area)", TCHAR_TO_UTF8(*GetNameSafe(Anchor->GetOwner())),
				Winch->GetAnchorAlpha(), FVector::Dist(Anchor->GetAreaPoint(0.0f), Winch->GetHookPoint()));
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
					FVector::Dist(Winch->GetComponentLocation(), Nearest->GetClosestAreaPoint(Winch->GetComponentLocation())) / 100.0f);
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

		if (Hook != EB1WinchHook::Carried)
		{
			DrawBar("Length m", Winch->GetCurrentLength() / 100.0f, Winch->MaxCableLength / 100.0f);
		}
		ImGui::Text("Rest %.2f m", Winch->GetRestLength() / 100.0f);
		DrawBar("Tension kN", Winch->GetTension() / 1000.0f, Winch->MaxPullForce / 1000.0f);
		DrawBar("Hold kN", Winch->GetHoldForce() / 1000.0f, Winch->HandbrakeHoldForce / 1000.0f);
		ImGui::SameLine();
		ImGui::TextColored(Winch->IsHoldActive() ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), Winch->IsHoldActive() ? "active" : "off");
		ImGui::Text("Pull %+4.0f %% (%s)", Winch->Pull * 100.0f, Winch->Pull > 0.0f ? "in" : Winch->Pull < 0.0f ? "out" : "hold");

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

		DrawLogSection(World, PlayerPawn);

		ImGui::End();
#endif
	}
}
