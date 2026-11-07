#include "Player/EchoCharacter.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

AEchoCharacter::AEchoCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

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
