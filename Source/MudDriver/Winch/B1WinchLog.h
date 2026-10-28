#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "B1WinchLog.generated.h"

class UStaticMeshComponent;
class UB1WinchAnchorComponent;

/** Loose log a winch can drag: a simulating trunk (mesh and mass set in BP) with an anchor on it */
UCLASS()
class MUDDRIVER_API AB1WinchLog : public AActor
{
	GENERATED_BODY()

public:

	AB1WinchLog();

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Winch|Components")
	TObjectPtr<UStaticMeshComponent> Trunk;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Winch|Components")
	TObjectPtr<UB1WinchAnchorComponent> Anchor;
};
