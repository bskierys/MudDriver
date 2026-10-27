#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "B1InteractionPromptWidget.generated.h"

class UInputAction;
class UTextBlock;

/** "[key] Take hook": the possessed character's best scan option, the same one GA_B1Interact sends (ADR-017) */
UCLASS()
class MUDDRIVER_API UB1InteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual bool Initialize() override;

	/** Its mapped keys label the prompt */
	void SetInteractAction(const UInputAction* Action) { InteractAction = Action; KeyText = FText::GetEmpty(); }

protected:

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	FText BuildKeyText() const;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(Transient)
	TObjectPtr<const UInputAction> InteractAction;

	FText KeyText;
};
