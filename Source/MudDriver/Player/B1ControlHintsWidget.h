#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "B1ControlHintsWidget.generated.h"

class UB1ControlHintSet;
class UTextBlock;

/** "W / RT  Drive" lines in the lower-left corner for the hints whose tags the player's ASC matches */
UCLASS()
class MUDDRIVER_API UB1ControlHintsWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual bool Initialize() override;

	void SetHintSet(const UB1ControlHintSet* NewHintSet) { HintSet = NewHintSet; }

	/** Current lines, for tests and captures */
	FString GetHintText() const;

protected:

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	void Refresh();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient)
	TObjectPtr<const UB1ControlHintSet> HintSet;

	/** Contexts change on possession and tags at any time, so rebuild a few times a second */
	float RefreshTimer = 0.0f;
	bool bLoggedMissing = false;
};
