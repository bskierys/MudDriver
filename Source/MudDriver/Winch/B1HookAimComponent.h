#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "B1HookAimComponent.generated.h"

class UB1WinchAnchorComponent;
class UB1WinchComponent;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * While its pawn carries a winch hook, aims the hook along the view ray onto an anchor area in range (ADR-019).
 * Only computes: the result goes to the carried winch (SetAimTarget). Shows a ghost ring there.
 */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1HookAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UB1HookAimComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Used when the owner has no UB1InteractionScanComponent; otherwise its Range, so aim and prompt agree */
	UPROPERTY(EditAnywhere, Category = "Winch|Aim", meta = (Units = "cm", ClampMin = "0"))
	float Range = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Winch|Aim")
	TObjectPtr<UMaterialInterface> GhostMaterial;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Best anchor and alpha for the view ray; false when nothing is in reach */
	bool FindAim(const UB1WinchComponent& Winch, UB1WinchAnchorComponent*& OutAnchor, float& OutAlpha) const;
	void ShowGhost(const UB1WinchComponent& Winch, const UB1WinchAnchorComponent* Target, float Alpha);
	void HideGhost();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ghost;
};
