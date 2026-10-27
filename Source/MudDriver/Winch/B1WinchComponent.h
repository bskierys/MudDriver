#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "B1WinchComponent.generated.h"

class UB1WinchAnchorComponent;
class UB1VehicleMovementComponent;
class UCableComponent;
class UMaterialInterface;

/**
 * Winch on the vehicle; the component's location is the winch point. Cable is a capped one-sided spring (ADR-007),
 * applied in FB1VehicleSimulation; this side sends the cable state and reads tension back.
 */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1WinchComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UB1WinchComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "cm", ClampMin = "0"))
	float MaxCableLength = 4000.0f;

	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "N", ClampMin = "0"))
	float MaxPullForce = 40000.0f;

	/** Rest length shrink rate at full pull; the drum stalls at MaxPullForce */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float ReelInSpeed = 100.0f;

	/** Reel-in stops here, short of the bumper */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "cm", ClampMin = "0"))
	float MinRestLength = 150.0f;

	/** N per m of stretch past the rest length */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (ClampMin = "0"))
	float CableStiffness = 200000.0f;

	/** N per m/s of stretch rate */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (ClampMin = "0"))
	float CableDamping = 20000.0f;

	/** Loose tag on the owner's ASC while reeling in; set by the host */
	UPROPERTY(EditAnywhere, Category = "Winch|Config")
	FGameplayTag WinchingTag;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (Units = "cm", ClampMin = "0.1"))
	float CableWidth = 7.0f;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (ClampMin = "1"))
	int32 CableSegments = 16;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual")
	TObjectPtr<UMaterialInterface> CableMaterial;

	/** Placed vehicles: hook this actor's anchor at BeginPlay */
	UPROPERTY(EditInstanceOnly, Category = "Winch|Debug")
	TObjectPtr<AActor> StartAnchor;

	/** Reel-in speed as a fraction of ReelInSpeed, [0, 1]. Input writes it every frame; editable for placed vehicles */
	UPROPERTY(EditAnywhere, Category = "Winch|Debug", meta = (ClampMin = "0", ClampMax = "1"))
	float Pull = 0.0f;

	/** Rest length = current distance. False when the anchor is out of MaxCableLength */
	UFUNCTION(BlueprintCallable, Category = "Winch")
	bool AttachTo(UB1WinchAnchorComponent* Anchor);

	UFUNCTION(BlueprintCallable, Category = "Winch")
	void Detach();

	/** Detaches when attached, else hooks the nearest anchor in range */
	void ToggleAttachNearest();

	UFUNCTION(BlueprintCallable, Category = "Winch")
	void SetPull(float NewPull) { Pull = FMath::Clamp(NewPull, 0.0f, 1.0f); }

	/** Nearest anchor within MaxCableLength in this world */
	UB1WinchAnchorComponent* FindNearestAnchor() const;

	bool IsAttached() const { return Anchor.IsValid(); }
	bool IsWinching() const { return IsAttached() && Pull > 0.0f; }
	UB1WinchAnchorComponent* GetAnchor() const { return Anchor.Get(); }

	/** Latest physics step; N */
	float GetTension() const { return Tension; }
	/** cm */
	float GetRestLength() const { return RestLength; }
	float GetCurrentLength() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void SendSimInput();
	void ReadSimOutput();
	void SetWinchingTag(bool bWinching);

	TWeakObjectPtr<UB1WinchAnchorComponent> Anchor;

	UPROPERTY(Transient)
	TObjectPtr<UB1VehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UCableComponent> Cable;

	uint32 AttachSerial = 0;
	float InitialRestLength = 0.0f;
	float RestLength = 0.0f;
	float Tension = 0.0f;
	float PeakTension = 0.0f;
	bool bWinchingTagSet = false;
};
