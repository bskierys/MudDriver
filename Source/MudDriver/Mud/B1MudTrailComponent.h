#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "B1MudTrailComponent.generated.h"

class UB1MudVehicleComponent;

/** Feeds each mud wheel's contact path and sink depth to UB1MudTrailSubsystem (ADR-008, visual only) */
UCLASS(ClassGroup = (B1), meta = (BlueprintSpawnableComponent))
class MUDDRIVER_API UB1MudTrailComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UB1MudTrailComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Rut floor width over the tire width */
	UPROPERTY(EditAnywhere, Category = "Mud|Trails", meta = (ClampMin = "0.1"))
	float WidthScale = 1.2f;

	/** Extra rut width per cm of depth, for sloped walls; deep ruts get wide, not trench-like */
	UPROPERTY(EditAnywhere, Category = "Mud|Trails", meta = (ClampMin = "0"))
	float WallWidthPerDepth = 1.5f;

	/** Wheels sunk less than this leave no rut */
	UPROPERTY(EditAnywhere, Category = "Mud|Trails", meta = (Units = "cm", ClampMin = "0"))
	float MinRutDepth = 2.0f;

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UB1MudVehicleComponent> MudVehicle;

	/** Per wheel; unset while the wheel is off mud */
	TArray<TOptional<FVector>> LastStamp;
};
