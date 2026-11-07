#include "Player/EchoCharacter.h"
#include "Player/EchoCharacterMovementComponent.h"
#include "Combat/EchoProjectile.h"
#include "Combat/EchoHealthComponent.h"
#include "Core/EchoGameMode.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ECHOSTRIKE.h"
#include "UObject/ConstructorHelpers.h"

AEchoCharacter::AEchoCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UEchoCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	SetReplicateMovement(true);
	HealthComponent = CreateDefaultSubobject<UEchoHealthComponent>(TEXT("HealthComponent"));

	// CharacterMovement predicts this rotation locally, simulates it on the server,
	// and smooths replicated movement/rotation on simulated proxies, even at rest.
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Movement->RotationRate = FRotator(0.0, 1080.0, 0.0);
	Movement->MaxWalkSpeed = 650.0f;
	Movement->MaxAcceleration = 6000.0f;
	Movement->BrakingDecelerationWalking = 6000.0f;
	Movement->GroundFriction = 8.0f;

	// These inherited component properties remain editable in the editor. Absolute
	// rotation decouples the camera from both character facing and control rotation.
	USpringArmComponent* Boom = GetCameraBoom();
	Boom->bUsePawnControlRotation = false;
	Boom->SetUsingAbsoluteRotation(true);
	Boom->SetRelativeRotation(FRotator(-55.0, 0.0, 0.0));
	Boom->TargetArmLength = 1200.0f;
	Boom->bDoCollisionTest = false; // Arena obstacles must not pump the camera zoom.
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 24.0f;
	Boom->CameraLagMaxDistance = 35.0f;
	Boom->bUseCameraLagSubstepping = true;
	Boom->bEnableCameraRotationLag = false;
	GetFollowCamera()->bUsePawnControlRotation = false;

	// Reuse the assets configured by BP_ThirdPersonCharacter without altering that asset.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimClass(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	GetMesh()->SetSkeletalMesh(MeshAsset.Object);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -90.0), FRotator(0.0, -90.0, 0.0));
	GetMesh()->SetAnimInstanceClass(AnimClass.Class);

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpInput(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveInput(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> LookInput(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookInput(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	JumpAction = JumpInput.Object;
	MoveAction = MoveInput.Object;
	LookAction = LookInput.Object;
	MouseLookAction = MouseLookInput.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> FireInput(TEXT("/Game/Input/Actions/IA_Fire.IA_Fire"));
	FireAction = FireInput.Object;
	static ConstructorHelpers::FObjectFinder<UInputAction> DashInput(TEXT("/Game/Input/Actions/IA_Dash.IA_Dash"));
	DashAction = DashInput.Object;
	ProjectileClass = AEchoProjectile::StaticClass();
}

void AEchoCharacter::DoMove(float Right, float Forward)
{
	if (Controller && IsLocallyControlled() && !IsEliminated())
	{
		// Never use control rotation here: it belongs to mouse aiming.
		const FRotator CameraYaw(0.0, GetCameraBoom()->GetComponentRotation().Yaw, 0.0);
		const FRotationMatrix CameraAxes(CameraYaw);
		AddMovementInput(CameraAxes.GetUnitAxis(EAxis::X), Forward);
		AddMovementInput(CameraAxes.GetUnitAxis(EAxis::Y), Right);
	}
}

void AEchoCharacter::DoLook(float Yaw, float Pitch)
{
	// Ignore the template's mouse/gamepad orbit actions. Only cursor aiming sets yaw.
}

void AEchoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent); Input && DashAction)
	{
		Input->BindAction(DashAction, ETriggerEvent::Started, this, &AEchoCharacter::RequestDash);
	}
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent); Input && FireAction)
	{
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &AEchoCharacter::StartFiring);
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &AEchoCharacter::StopFiring);
		Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &AEchoCharacter::StopFiring);
	}
}

void AEchoCharacter::RequestDash()
{
	if (IsLocallyControlled() && !IsEliminated())
	{
		CastChecked<UEchoCharacterMovementComponent>(GetCharacterMovement())->RequestDash();
	}
}

float AEchoCharacter::GetFireInterval() const
{
	return 1.0f / FMath::Clamp(FMath::IsFinite(FireRate) ? FireRate : 5.0f, 0.1f, 20.0f);
}

void AEchoCharacter::StartFiring()
{
	if (IsLocallyControlled() && !IsEliminated())
	{
		ServerSetFiring(true);
	}
}

void AEchoCharacter::StopFiring()
{
	if (IsLocallyControlled())
	{
		ServerSetFiring(false);
	}
}

void AEchoCharacter::ServerSetFiring_Implementation(bool bFiring)
{
	const APlayerController* Player = Cast<APlayerController>(Controller);
	if (!HasAuthority() || !Player || Player->GetPawn() != this || GetOwner() != Player)
	{
		return;
	}
	if (!bFiring || IsEliminated())
	{
		GetWorldTimerManager().ClearTimer(FireTimer);
	}
	else if (!GetWorldTimerManager().IsTimerActive(FireTimer))
	{
		FireShot();
	}
}

void AEchoCharacter::FireShot()
{
	const APlayerController* Player = Cast<APlayerController>(Controller);
	if (!HasAuthority() || IsEliminated() || !Player || Player->GetPawn() != this || GetOwner() != Player || !ProjectileClass)
	{
		GetWorldTimerManager().ClearTimer(FireTimer);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now + UE_KINDA_SMALL_NUMBER < NextServerShotTime)
	{
		GetWorldTimerManager().SetTimer(FireTimer, this, &AEchoCharacter::FireShot,
			static_cast<float>(NextServerShotTime - Now), false);
		return;
	}
	NextServerShotTime = Now + GetFireInterval();
	// One timer on the authority, rather than a reliable RPC for every held shot.
	GetWorldTimerManager().SetTimer(FireTimer, this, &AEchoCharacter::FireShot, GetFireInterval(), false);

	// No client-provided transform or tuning values cross the RPC boundary.
	const FRotator Facing(0.0, GetActorRotation().Yaw, 0.0);
	const FVector Origin = GetActorLocation();
	const FVector SpawnLocation = Origin + Facing.RotateVector(ProjectileSpawnOffset);
	if (Origin.ContainsNaN() || Facing.ContainsNaN() || SpawnLocation.ContainsNaN()
		|| !FMath::IsFinite(ProjectileSpeed) || ProjectileSpeed <= 0.0f
		|| !FMath::IsFinite(ProjectileLifetime) || ProjectileLifetime <= 0.0f)
	{
		return;
	}

	// Sweep the complete offset so firing beside cover cannot spawn beyond a wall.
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EchoProjectileSpawn), false, this);
	FHitResult Obstruction;
	const float Radius = ProjectileClass.GetDefaultObject()->GetCollisionRadius();
	if (GetWorld()->SweepSingleByProfile(Obstruction, Origin, SpawnLocation, FQuat::Identity,
		TEXT("BlockAllDynamic"), FCollisionShape::MakeSphere(Radius), Query))
	{
		UE_LOG(LogECHOSTRIKE, Log, TEXT("Shot from %s blocked by %s at %s"),
			*GetName(), *GetNameSafe(Obstruction.GetActor()), *Obstruction.ImpactPoint.ToCompactString());
		return;
	}

	const FTransform SpawnTransform(Facing, SpawnLocation);
	AEchoProjectile* Projectile = GetWorld()->SpawnActorDeferred<AEchoProjectile>(
		ProjectileClass, SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->InitializeFlight(ProjectileSpeed, ProjectileLifetime);
		Projectile->FinishSpawning(SpawnTransform);
	}
}

void AEchoCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FireTimer);
	Super::EndPlay(EndPlayReason);
}

void AEchoCharacter::UnPossessed()
{
	GetWorldTimerManager().ClearTimer(FireTimer);
	Super::UnPossessed();
}

void AEchoCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	HealthComponent->OnEliminated.AddDynamic(this, &AEchoCharacter::HandleEliminated);
}

bool AEchoCharacter::IsEliminated() const
{
	return HealthComponent->IsEliminated();
}

bool AEchoCharacter::CanJumpInternal_Implementation() const
{
	return !IsEliminated() && Super::CanJumpInternal_Implementation();
}

void AEchoCharacter::HandleEliminated(UEchoHealthComponent* Component)
{
	// Actor hiding covers every owned primitive without changing component defaults
	// (or deactivating the owning player's camera during the respawn delay).
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetWorldTimerManager().ClearTimer(FireTimer);
	StopJumping();
	ConsumeMovementInputVector();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	if (HasAuthority())
	{
		// Hidden, non-colliding pawns otherwise cease being relevant to other players
		// before their final health/visibility state can reach those connections.
		bAlwaysRelevant = true;
		ForceNetUpdate();
		if (AEchoGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoGameMode>())
		{
			GameMode->RequestRespawn(this);
		}
	}
	OnEliminated();
}
