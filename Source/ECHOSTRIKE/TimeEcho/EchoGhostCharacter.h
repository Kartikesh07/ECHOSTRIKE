#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoGhostCharacter.generated.h"

class USkeletalMeshComponent;
class UEchoPlaybackComponent;

/** Visual duplicate only: deliberately has no pawn, movement, health or combat systems. */
UCLASS(Blueprintable)
class ECHOSTRIKE_API AEchoGhostCharacter : public AActor
{
	GENERATED_BODY()
public:
	AEchoGhostCharacter();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time Echo")
	TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time Echo")
	TObjectPtr<UEchoPlaybackComponent> Playback;
protected:
	virtual void BeginPlay() override;
};
