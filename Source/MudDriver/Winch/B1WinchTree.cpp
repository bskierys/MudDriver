#include "B1WinchTree.h"
#include "B1WinchAnchorComponent.h"
#include "Components/StaticMeshComponent.h"

AB1WinchTree::AB1WinchTree()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Trunk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trunk"));
	Trunk->SetupAttachment(RootComponent);
	Trunk->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Anchor = CreateDefaultSubobject<UB1WinchAnchorComponent>(TEXT("Anchor"));
	Anchor->SetupAttachment(RootComponent);
	Anchor->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
}
