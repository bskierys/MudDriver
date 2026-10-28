#include "B1InteractionPromptWidget.h"
#include "B1Character.h"
#include "B1InteractionScanComponent.h"
#include "B1InputKeyText.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"

bool UB1InteractionPromptWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// Built in code, so there's no widget BP to keep in sync.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
		FSlateFontInfo Font = PromptText->GetFont();
		Font.Size = 20;
		PromptText->SetFont(Font);
		PromptText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		PromptText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		PromptText->SetJustification(ETextJustify::Center);
		PromptText->SetVisibility(ESlateVisibility::Collapsed);

		UCanvasPanelSlot* PromptSlot = Root->AddChildToCanvas(PromptText);
		PromptSlot->SetAnchors(FAnchors(0.5f, 0.62f));
		PromptSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PromptSlot->SetAutoSize(true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UB1InteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	const AB1Character* Character = PC ? Cast<AB1Character>(PC->GetPawn()) : nullptr;
	const UB1InteractionScanComponent* Scan = Character ? Character->GetInteractionScan() : nullptr;
	const FB1InteractionOption* Option = Scan ? Scan->GetBestOption() : nullptr;
	if (!PromptText)
	{
		return;
	}
	if (!Option)
	{
		PromptText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (KeyText.IsEmpty())
	{
		KeyText = BuildKeyText();
	}
	PromptText->SetText(FText::Format(NSLOCTEXT("B1Interaction", "Prompt", "[{0}]  {1}"), KeyText, Option->Text));
	PromptText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

FText UB1InteractionPromptWidget::BuildKeyText() const
{
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = GetOwningLocalPlayer() ? GetOwningLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem || !InteractAction)
	{
		return NSLOCTEXT("B1Interaction", "NoKey", "?");
	}
	return B1InputKeyText::GetKeyText(*Subsystem, { InteractAction.Get() });
}
