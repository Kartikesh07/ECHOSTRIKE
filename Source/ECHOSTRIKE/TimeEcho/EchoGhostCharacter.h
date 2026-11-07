#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoGhostCharacter.generated.h"

class USkeletalMeshComponent;
class UEchoPlaybackComponent;

/** Visual duplicate only: deliberately has no pawn, movement, collision or health systems. */
UCLASS(Blueprintable)
class ECHOSTRIKE_API AEchoGhostCharacter : public AActor
{
	GENERATED_BODY()
public:
	AEchoGhostCharacter();
	/** Cosmetic only. Transform playback already contains dash displacement. Never launch/move the actor here. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Time Echo|Presentation")
	void OnDashReplay(const FTransform& RecordedTransform, FVector Direction, float ElapsedSeconds);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time Echo")
	TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time Echo")
	TObjectPtr<UEchoPlaybackComponent> Playback;
protected:
	virtual void BeginPlay() override;
};
