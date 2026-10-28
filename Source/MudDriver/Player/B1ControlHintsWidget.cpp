#include "B1ControlHintsWidget.h"
#include "B1ControlHintSet.h"
#include "B1InputKeyText.h"
#include "B1Log.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
	constexpr float RefreshInterval = 0.2f;
}

bool UB1ControlHintsWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// Built in code like the interaction prompt. The root stays visible: a collapsed UUserWidget stops ticking.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
		FSlateFontInfo Font = HintText->GetFont();
		Font.Size = 16;
		HintText->SetFont(Font);
		HintText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		HintText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		HintText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f)));

		UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(HintText);
		HintSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		HintSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		HintSlot->SetPosition(FVector2D(24.0f, -24.0f));
		HintSlot->SetAutoSize(true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UB1ControlHintsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshTimer -= InDeltaTime;
	if (RefreshTimer <= 0.0f)
	{
		RefreshTimer = RefreshInterval;
		Refresh();
	}
}

void UB1ControlHintsWidget::Refresh()
{
	const APlayerController* PC = GetOwningPlayer();
	const UAbilitySystemComponent* ASC = PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->PlayerState) : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = GetOwningLocalPlayer() ? GetOwningLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!HintText || !HintSet || !ASC || !Subsystem)
	{
		UE_CLOG(!bLoggedMissing, LogB1Vehicle, Warning, TEXT("controls hints off: text %d, hint set %d, ASC %d, input subsystem %d"),
			HintText != nullptr, HintSet != nullptr, ASC != nullptr, Subsystem != nullptr);
		bLoggedMissing = true;
		if (HintText)
		{
			HintText->SetText(FText::GetEmpty());
		}
		return;
	}

	const FGameplayTagContainer& Tags = ASC->GetOwnedGameplayTags();
	TArray<FString> Lines;
	for (const FB1ControlHint& Hint : HintSet->Hints)
	{
		if (!Tags.HasAll(Hint.RequiredTags) || Tags.HasAny(Hint.BlockedTags))
		{
			continue;
		}
		TArray<const UInputAction*, TInlineAllocator<4>> Actions;
		for (const TObjectPtr<UInputAction>& Action : Hint.Actions)
		{
			Actions.Add(Action.Get());
		}
		const FText Keys = B1InputKeyText::GetKeyText(*Subsystem, Actions, Hint.KeysPerDevice);
		Lines.Add(FString::Printf(TEXT("%s   %s"), *Keys.ToString(), *Hint.Label.ToString()));
	}
	const FString Text = FString::Join(Lines, TEXT("\n"));
	if (Text != HintText->GetText().ToString())
	{
		UE_LOG(LogB1Vehicle, Log, TEXT("controls hints: %s"), *Text.Replace(TEXT("\n"), TEXT(" | ")));
		HintText->SetText(FText::FromString(Text));
	}
}

FString UB1ControlHintsWidget::GetHintText() const
{
	return HintText ? HintText->GetText().ToString() : FString();
}
