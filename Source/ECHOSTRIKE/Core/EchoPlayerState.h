#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "EchoPlayerState.generated.h"

/** Uses the framework's replicated player identity and connection lifecycle. */
UCLASS()
class ECHOSTRIKE_API AEchoPlayerState : public APlayerState
{
	GENERATED_BODY()
};
