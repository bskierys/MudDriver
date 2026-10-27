#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "B1Character.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;

/** First-person on-foot avatar; its ASC is the PlayerState's (ADR-004) */
UCLASS(abstract)
class MUDDRIVER_API AB1Character : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AB1Character(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** World-space bottom of the capsule */
	float GetCapsuleBottomZ() const;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// Mapping contexts are swapped by AB1PlayerController on possession.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookMouseAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookStickAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Scales raw mouse look before the controller's input scale */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0"))
	float MouseLookScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Units = "deg/s", ClampMin = "0"))
	float StickYawRate = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Units = "deg/s", ClampMin = "0"))
	float StickPitchRate = 165.0f;

private:

	void Move(const FInputActionValue& Value);
	void LookMouse(const FInputActionValue& Value);
	void LookStick(const FInputActionValue& Value);
};
