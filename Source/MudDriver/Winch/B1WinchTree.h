#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "B1WinchTree.generated.h"

class UStaticMeshComponent;
class UB1WinchAnchorComponent;

/** Placeholder tree: a trunk mesh (set in BP) with a winch anchor area at strap height */
UCLASS()
class MUDDRIVER_API AB1WinchTree : public AActor
{
	GENERATED_BODY()

public:

	AB1WinchTree();

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Winch|Components")
	TObjectPtr<UStaticMeshComponent> Trunk;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Winch|Components")
	TObjectPtr<UB1WinchAnchorComponent> Anchor;
};
