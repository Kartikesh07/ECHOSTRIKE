#include "Player/EchoCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

AEchoCharacter::AEchoCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

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
}

void AEchoCharacter::DoMove(float Right, float Forward)
{
	if (Controller && IsLocallyControlled())
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
