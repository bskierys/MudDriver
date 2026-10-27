#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "B1FuelLoadSource.h"
#include "B1FuelConsumptionComponent.generated.h"

class UAbilitySystemComponent;
class UB1FuelConsumptionData;
class UGameplayEffect;
class IB1EngineLoadSource;
class APawn;
class AController;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_OneParam(FB1OnOutOfFuelChanged, bool /*bOutOfFuel*/);

/**
 * ADR-010: while the owner pawn is driven, burns IdleRate + WorkRate × engine power + Σ load rates × loads (L/h)
 * from FuelAttribute on the owner's ASC, applied as one DrainEffect per DrainInterval.
 */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1FuelConsumptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UB1FuelConsumptionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UB1FuelConsumptionData* GetData() const { return Data; }

	/** L/h this frame; 0 while nobody drives */
	float GetFuelRate() const { return FuelRate; }

	/** Over the last few seconds; negative until the vehicle has moved at least a metre in that window */
	float GetFuelPer100m() const;

	/** Engine power used for the current rate, [0, 1] */
	float GetEnginePower() const { return EnginePower; }

	/** Load with this name used for the current rate; 0 when absent or not consuming */
	float GetLoad(FName Name) const;
	const TArray<FB1FuelLoad>& GetLoads() const { return Loads; }

	bool IsOutOfFuel() const { return bOutOfFuel; }

	/** The vehicle cuts its engine here */
	FB1OnOutOfFuelChanged OnOutOfFuelChanged;

	UPROPERTY(EditAnywhere, Category = "Fuel|Config")
	TObjectPtr<UB1FuelConsumptionData> Data;

	UPROPERTY(EditAnywhere, Category = "Fuel|Config")
	FGameplayAttribute FuelAttribute;

	/** Instant effect with a SetByCaller modifier on FuelAttribute; gets the signed litres (negative) */
	UPROPERTY(EditAnywhere, Category = "Fuel|Config")
	TSubclassOf<UGameplayEffect> DrainEffect;

	UPROPERTY(EditAnywhere, Category = "Fuel|Config")
	FGameplayTag DrainSetByCallerTag;

	/** Loose tag on the ASC while FuelAttribute is 0; none set means no tag */
	UPROPERTY(EditAnywhere, Category = "Fuel|Config")
	FGameplayTag OutOfFuelTag;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void ConsumeFuel(float DeltaTime, UAbilitySystemComponent& ASC);
	void ApplyDrain(UAbilitySystemComponent& ASC, float Litres) const;

	/** Binds the fuel change delegate on the driver's ASC; it's unbound when the controller changes */
	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	void UnbindAbilitySystem();

	UFUNCTION()
	void OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void OnFuelChanged(const FOnAttributeChangeData& Change);
	void UpdateOutOfFuel(float Fuel);

	IB1EngineLoadSource* EngineSource = nullptr;
	TArray<const IB1FuelLoadSource*> LoadSources;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle FuelChangedHandle;

	bool bOutOfFuel = false;
	float FuelRate = 0.0f;
	float EnginePower = 0.0f;
	TArray<FB1FuelLoad> Loads;
	float PendingDrain = 0.0f;
	float DrainTimer = 0.0f;
	FVector LastDrainLocation = FVector::ZeroVector;

	/** One entry per drain: litres and metres */
	TArray<FVector2f> FuelWindow;
};
