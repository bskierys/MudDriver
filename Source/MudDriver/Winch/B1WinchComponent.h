#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "B1WinchComponent.generated.h"

class UB1WinchAnchorComponent;
class UB1VehicleMovementComponent;
class UCableComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EB1WinchHook : uint8
{
	/** On the drum, no cable */
	Stowed,
	/** In someone's hand: visual cable only, no tension */
	Carried,
	Attached,
};

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

	/** Rest length growth rate at full reel-out, up to MaxCableLength; not limited by the stall */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float ReelOutSpeed = 100.0f;

	/** Reel-in stops here, short of the bumper */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "cm", ClampMin = "0"))
	float MinRestLength = 150.0f;

	/** N per m of stretch past the rest length */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (ClampMin = "0"))
	float CableStiffness = 200000.0f;

	/** N per m/s of stretch rate */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (ClampMin = "0"))
	float CableDamping = 20000.0f;

	/** Cap of the standstill hold while hooked with the handbrake on; cancels the cable pull and slope gravity */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "N", ClampMin = "0"))
	float HandbrakeHoldForce = 80000.0f;

	/** Below this ground speed the hold also cancels slope gravity and drift; above it, only the cable pull */
	UPROPERTY(EditAnywhere, Category = "Winch|Config", meta = (Units = "cm/s", ClampMin = "0"))
	float HandbrakeHoldSpeed = 30.0f;

	/** Loose tag on the owner's ASC while reeling in; set by the host */
	UPROPERTY(EditAnywhere, Category = "Winch|Config")
	FGameplayTag WinchingTag;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (Units = "cm", ClampMin = "0.1"))
	float CableWidth = 7.0f;

	/** Segments are sized for MaxCableLength, so a fully reeled-out cable can still lie on the ground */
	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (Units = "cm", ClampMin = "10"))
	float CableSegmentLength = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (ClampMin = "1"))
	int32 CableSolverIterations = 16;

	/** Visual cable rests on WorldStatic, WorldDynamic and the mud surface; vehicles and pawns are ignored */
	UPROPERTY(EditAnywhere, Category = "Winch|Visual")
	bool bCableCollision = true;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (ClampMin = "0", ClampMax = "1"))
	float CableCollisionFriction = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual")
	TObjectPtr<UMaterialInterface> CableMaterial;

	/** Ring shown around the trunk while attached; a 100 cm unit cylinder along Z */
	UPROPERTY(EditAnywhere, Category = "Winch|Visual")
	TObjectPtr<UStaticMesh> StrapMesh;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual")
	TObjectPtr<UMaterialInterface> StrapMaterial;

	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (Units = "cm", ClampMin = "0"))
	float StrapWidth = 15.0f;

	/** Strap radius past the anchor's TrunkRadius */
	UPROPERTY(EditAnywhere, Category = "Winch|Visual", meta = (Units = "cm", ClampMin = "0"))
	float StrapClearance = 2.0f;

	/** Placed vehicles: hook this actor's anchor at BeginPlay */
	UPROPERTY(EditInstanceOnly, Category = "Winch|Debug")
	TObjectPtr<AActor> StartAnchor;

	/** Where on StartAnchor's area, 0..1 */
	UPROPERTY(EditInstanceOnly, Category = "Winch|Debug", meta = (ClampMin = "0", ClampMax = "1"))
	float StartAnchorAlpha = 0.5f;

	/** [-1, 1]: positive reels in (fraction of ReelInSpeed), negative reels out. Input writes it every frame; editable for placed vehicles */
	UPROPERTY(EditAnywhere, Category = "Winch|Debug", meta = (ClampMin = "-1", ClampMax = "1"))
	float Pull = 0.0f;

	/**
	 * Hooks the anchor's area at AreaAlpha (negative = the point nearest the winch); rest length = current distance.
	 * Works from any hook state. False when that point is past MaxCableLength.
	 */
	UFUNCTION(BlueprintCallable, Category = "Winch")
	bool AttachTo(UB1WinchAnchorComponent* Anchor, float AreaAlpha = -1.0f);

	/** Hook back on the drum from any state */
	UFUNCTION(BlueprintCallable, Category = "Winch")
	void Detach();

	/** Stowed hook to Hand. A carrying ACharacter is leashed: pulled back onto MaxCableLength from the winch point */
	bool TakeHook(USceneComponent* Hand);

	/** Attached hook off its anchor and into Hand */
	bool DetachToCarrier(USceneComponent* Hand);

	EB1WinchHook GetHookState() const;
	AActor* GetHookCarrier() const { return HookCarrier.IsValid() ? HookCarrier->GetOwner() : nullptr; }
	bool IsInReach(const UB1WinchAnchorComponent* Target) const;

	static UB1WinchComponent* FindCarriedBy(const AActor* Carrier);
	static UB1WinchComponent* FindAttachedTo(const UB1WinchAnchorComponent* Target);

	/** Detaches when attached, else hooks the nearest anchor in range */
	void ToggleAttachNearest();

	UFUNCTION(BlueprintCallable, Category = "Winch")
	void SetPull(float NewPull) { Pull = FMath::Clamp(NewPull, -1.0f, 1.0f); }

	/** Nearest anchor within MaxCableLength in this world */
	UB1WinchAnchorComponent* FindNearestAnchor() const;

	bool IsAttached() const { return Anchor.IsValid(); }
	bool IsWinching() const { return IsAttached() && Pull > 0.0f; }
	UB1WinchAnchorComponent* GetAnchor() const { return Anchor.Get(); }
	float GetAnchorAlpha() const { return AnchorAlpha; }
	/** On the trunk axis, where the sim pulls; the winch point when not attached */
	FVector GetHookPoint() const;

	/** Latest physics step; N */
	float GetTension() const { return Tension; }
	/** Latest physics step; N */
	float GetHoldForce() const { return HoldForce; }
	bool IsHoldActive() const { return bHoldActive; }
	/** cm */
	float GetRestLength() const { return RestLength; }
	/** Winch point to the anchor or the carrier's hand, cm */
	float GetCurrentLength() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void SendSimInput();
	void ReadSimOutput();
	void SetWinchingTag(bool bWinching);
	void SetupCableCollision(UCableComponent& CableComponent) const;
	void CreateCable(USceneComponent& End, float Length);
	/** Ends the anchor's sim cable; keeps the visual cable */
	void ReleaseAnchor();
	void CarryTo(USceneComponent& Hand, float Length);
	void TickCarriedHook();
	/** Cable end on the strap's surface, on the side facing the winch */
	void UpdateAttachedCableEnd();
	void CreateStrap(UB1WinchAnchorComponent& Target);

	TWeakObjectPtr<UB1WinchAnchorComponent> Anchor;
	float AnchorAlpha = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Strap;
	TWeakObjectPtr<USceneComponent> HookCarrier;

	/** Visual cable paid out while carried, cm */
	float CarryLength = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UB1VehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<UCableComponent> Cable;

	uint32 AttachSerial = 0;
	float InitialRestLength = 0.0f;
	float RestLength = 0.0f;
	float Tension = 0.0f;
	float PeakTension = 0.0f;
	float HoldForce = 0.0f;
	bool bHoldActive = false;
	bool bWinchingTagSet = false;
};
