#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "EchoPlayerState.generated.h"

class UEchoRecorderComponent;

/** Uses the framework's replicated player identity and connection lifecycle. */
UCLASS()
class ECHOSTRIKE_API AEchoPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	AEchoPlayerState();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Time Echo")
	TObjectPtr<UEchoRecorderComponent> EchoRecorder;
};
