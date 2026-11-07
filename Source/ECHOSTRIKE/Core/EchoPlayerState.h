#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "EchoPlayerState.generated.h"

class UEchoRecorderComponent;
class AEchoGhostCharacter;
struct FEchoRecording;

/** Uses the framework's replicated player identity and connection lifecycle. */
UCLASS()
class ECHOSTRIKE_API AEchoPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	AEchoPlayerState();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time Echo")
	TObjectPtr<UEchoRecorderComponent> EchoRecorder;
	/** Visual class, including mesh/material defaults, is selected in Blueprint assets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Time Echo")
	TSubclassOf<AEchoGhostCharacter> EchoGhostClass;
	void ClearEcho();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void HandleCycleCompleted(const FEchoRecording& Recording, int64 NewCycle, double ServerStartTime);
	UPROPERTY(Transient) TObjectPtr<AEchoGhostCharacter> ActiveEcho;
};
