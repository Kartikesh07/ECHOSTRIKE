#include "Combat/EchoProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "ECHOSTRIKE.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AEchoProjectile::AEchoProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(8.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(false);
	Collision->OnComponentHit.AddDynamic(this, &AEchoProjectile::OnImpact);
	Collision->CanCharacterStepUpOn = ECB_No;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeScale3D(FVector(0.16f));
	Visual->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Visual->SetStaticMesh(Sphere.Object);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bShouldBounce = false;
	Movement->bRotationFollowsVelocity = true;
	Movement->InitialSpeed = 0.0f;
	Movement->Velocity = FVector::ZeroVector;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->MaxSpeed = 0.0f; // Velocity comes from authoritative initialization/replication.
}

AEchoProjectile* AEchoProjectile::SpawnAuthoritative(AActor* ShotOwner, APawn* InstigatorPawn,
	APlayerState* Player, TSubclassOf<AEchoProjectile> Class, const FTransform& Transform,
	float Speed, float Lifetime)
{
	if (!IsValid(ShotOwner) || !ShotOwner->HasAuthority() || !Class || Transform.ContainsNaN()
		|| !Transform.GetRotation().IsNormalized() || !FMath::IsFinite(Speed) || Speed <= 0.f
		|| !FMath::IsFinite(Lifetime) || Lifetime <= 0.f) return nullptr;
	AEchoProjectile* Projectile = ShotOwner->GetWorld()->SpawnActorDeferred<AEchoProjectile>(
		Class, Transform, ShotOwner, InstigatorPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) return nullptr;
	Projectile->SourcePlayer = Player;
	Projectile->InitializeFlight(Speed, Lifetime);
	Projectile->FinishSpawning(Transform);
	return IsValid(Projectile) ? Projectile : nullptr;
}

void AEchoProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AEchoProjectile, SourcePlayer, COND_InitialOnly);
}

void AEchoProjectile::HandleSourcePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	// Identity outlives the recorded pawn: ignore the player's current and future lives.
	if (IsValid(NewPawn)) Collision->IgnoreActorWhenMoving(NewPawn, true);
}

void AEchoProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(SourcePlayer)) SourcePlayer->OnPawnSet.RemoveDynamic(this, &AEchoProjectile::HandleSourcePawnSet);
	Super::EndPlay(Reason);
}

void AEchoProjectile::InitializeFlight(float Speed, float Lifetime)
{
	check(HasAuthority());
	Movement->Velocity = GetActorForwardVector() * Speed;
	SetLifeSpan(Lifetime);
}

float AEchoProjectile::GetCollisionRadius() const
{
	return Collision->GetScaledSphereRadius();
}

void AEchoProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Collision->IgnoreActorWhenMoving(GetOwner(), true);
		Collision->IgnoreActorWhenMoving(GetInstigator(), true);
		if (IsValid(SourcePlayer))
		{
			SourcePlayer->OnPawnSet.AddDynamic(this, &AEchoProjectile::HandleSourcePawnSet);
			HandleSourcePawnSet(SourcePlayer, SourcePlayer->GetPawn(), nullptr);
		}
	}
	else
	{
		// Clients extrapolate replicated velocity but never decide hits or destruction.
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AEchoProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	Super::PostNetReceiveVelocity(NewVelocity);
	Movement->Velocity = NewVelocity;
}

void AEchoProjectile::OnImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HasAuthority() && !bImpactHandled && IsValid(OtherActor)
		&& OtherActor != GetOwner() && OtherActor != GetInstigator()
		&& !(IsValid(SourcePlayer) && Cast<APawn>(OtherActor)
			&& Cast<APawn>(OtherActor)->GetPlayerState() == SourcePlayer))
	{
		// Mark consumed before damage callbacks, which can trigger further collision/destruction.
		bImpactHandled = true;
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (FMath::IsFinite(Damage) && Damage > 0.0f)
		{
			UGameplayStatics::ApplyPointDamage(OtherActor, Damage, GetActorForwardVector(), Hit,
				IsValid(SourcePlayer) ? SourcePlayer->GetPlayerController() : GetInstigatorController(), this, UDamageType::StaticClass());
		}
		UE_LOG(LogECHOSTRIKE, Log, TEXT("Projectile %s hit %s at %s"),
			*GetName(), *GetNameSafe(OtherActor), *Hit.ImpactPoint.ToCompactString());
		Destroy();
	}
}
