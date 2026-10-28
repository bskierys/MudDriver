#include "B1WinchLog.h"
#include "B1WinchAnchorComponent.h"
#include "Components/StaticMeshComponent.h"

AB1WinchLog::AB1WinchLog()
{
	Trunk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trunk"));
	RootComponent = Trunk;
	Trunk->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Trunk->SetSimulatePhysics(true);

	Anchor = CreateDefaultSubobject<UB1WinchAnchorComponent>(TEXT("Anchor"));
	Anchor->SetupAttachment(Trunk);
}
