#include "TimeEcho/EchoGhostCharacter.h"
#include "TimeEcho/EchoPlaybackComponent.h"
#include "Components/SkeletalMeshComponent.h"

AEchoGhostCharacter::AEchoGhostCharacter()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->CastShadow = false;
	Playback = CreateDefaultSubobject<UEchoPlaybackComponent>(TEXT("Playback"));
	SetActorEnableCollision(false);
}

void AEchoGhostCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Enforce the non-interactive contract even if a visual Blueprint changes mesh defaults.
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
	Mesh->SetSimulatePhysics(false);
}
