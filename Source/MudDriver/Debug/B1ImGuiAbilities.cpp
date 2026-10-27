#include "B1ImGuiAbilities.h"
#include "B1Character.h"
#include "B1InteractionScanComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "NetImguiModule.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace B1ImGui
{
#if NETIMGUI_ENABLED
	static constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
	static const ImVec4 ActiveColor(0.4f, 1.0f, 0.4f, 1.0f);

	static void DrawSpecs(UAbilitySystemComponent& ASC)
	{
		if (!ImGui::BeginTable("Specs", 4, TableFlags))
		{
			return;
		}

		ImGui::TableSetupColumn("Ability");
		ImGui::TableSetupColumn("Input tag");
		ImGui::TableSetupColumn("Active");
		ImGui::TableSetupColumn("Pressed");
		ImGui::TableHeadersRow();

		for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (Spec.IsActive())
			{
				ImGui::TextColored(ActiveColor, "%s", TCHAR_TO_UTF8(*GetNameSafe(Spec.Ability ? Spec.Ability->GetClass() : nullptr)));
			}
			else
			{
				ImGui::Text("%s", TCHAR_TO_UTF8(*GetNameSafe(Spec.Ability ? Spec.Ability->GetClass() : nullptr)));
			}
			ImGui::TableNextColumn(); ImGui::Text("%s", TCHAR_TO_UTF8(*Spec.GetDynamicSpecSourceTags().ToStringSimple()));
			ImGui::TableNextColumn(); ImGui::Text("%d", Spec.ActiveCount);
			ImGui::TableNextColumn(); ImGui::Text("%s", Spec.InputPressed ? "yes" : "");
		}

		ImGui::EndTable();
	}

	static void DrawTags(const UAbilitySystemComponent& ASC)
	{
		const FGameplayTagContainer& Owned = ASC.GetOwnedGameplayTags();
		if (Owned.IsEmpty())
		{
			ImGui::TextDisabled("none");
			return;
		}
		for (const FGameplayTag& Tag : Owned)
		{
			ImGui::BulletText("%s  x%d", TCHAR_TO_UTF8(*Tag.ToString()), ASC.GetTagCount(Tag));
		}
	}

	static void DrawOptions(const AB1Character& Character)
	{
		const UB1InteractionScanComponent* Scan = Character.GetInteractionScan();
		if (!Scan || Scan->GetOptions().IsEmpty())
		{
			ImGui::TextDisabled("none in range");
			return;
		}
		if (!ImGui::BeginTable("Options", 5, TableFlags))
		{
			return;
		}

		ImGui::TableSetupColumn("Text");
		ImGui::TableSetupColumn("Event");
		ImGui::TableSetupColumn("Prio");
		ImGui::TableSetupColumn("Dist m");
		ImGui::TableSetupColumn("Target");
		ImGui::TableHeadersRow();

		// Sorted best first, so row 0 is what interact does.
		const TArray<FB1InteractionOption>& Options = Scan->GetOptions();
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			const FB1InteractionOption& Option = Options[Index];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (Index == 0)
			{
				ImGui::TextColored(ActiveColor, "%s", TCHAR_TO_UTF8(*Option.Text.ToString()));
			}
			else
			{
				ImGui::Text("%s", TCHAR_TO_UTF8(*Option.Text.ToString()));
			}
			ImGui::TableNextColumn(); ImGui::Text("%s", TCHAR_TO_UTF8(*Option.TriggerEventTag.ToString()));
			ImGui::TableNextColumn(); ImGui::Text("%d", Option.Priority);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", FVector::Dist(Character.GetActorLocation(), Option.InteractionPoint) / 100.0f);
			ImGui::TableNextColumn(); ImGui::Text("%s", TCHAR_TO_UTF8(*GetNameSafe(Option.TargetActor.Get())));
		}

		ImGui::EndTable();
	}
#endif

	void DrawAbilities(UWorld* World, bool* bOpen)
	{
#if NETIMGUI_ENABLED
		if (!ImGui::Begin("Abilities", bOpen))
		{
			ImGui::End();
			return;
		}

		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC ? PC->PlayerState : nullptr);
		if (!ASC)
		{
			ImGui::TextDisabled("No ASC on the player state");
			ImGui::End();
			return;
		}

		ImGui::Text("Pawn: %s (%s)", TCHAR_TO_UTF8(*GetNameSafe(Pawn)), TCHAR_TO_UTF8(*GetNameSafe(Pawn ? Pawn->GetClass() : nullptr)));
		ImGui::Text("Avatar: %s", TCHAR_TO_UTF8(*GetNameSafe(ASC->GetAvatarActor())));

		if (ImGui::CollapsingHeader("Granted abilities", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawSpecs(*ASC);
		}
		if (ImGui::CollapsingHeader("Owned tags", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawTags(*ASC);
		}
		if (ImGui::CollapsingHeader("Interaction options", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (const AB1Character* Character = Cast<AB1Character>(Pawn))
			{
				DrawOptions(*Character);
			}
			else
			{
				ImGui::TextDisabled("on foot only");
			}
		}

		ImGui::End();
#endif
	}
}
