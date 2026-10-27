#include "B1TestInteractable.h"
#include "B1GameplayTags.h"
#include "Components/SphereComponent.h"

AB1TestInteractable::AB1TestInteractable()
{
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(30.0f);
	Sphere->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	Sphere->bHiddenInGame = false;
	RootComponent = Sphere;
}

void AB1TestInteractable::GatherInteractionOptions(const AB1Character& Character, TArray<FB1InteractionOption>& Options) const
{
	FB1InteractionOption& Option = Options.AddDefaulted_GetRef();
	Option.Text = NSLOCTEXT("B1Test", "Ping", "Ping");
	Option.TriggerEventTag = B1GameplayTags::Event_Interact_TestPing;
	Option.InteractionPoint = GetActorLocation();
}
