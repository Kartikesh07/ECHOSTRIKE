#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "EchoGameState.generated.h"

/** Replicated global framework state, including the connected PlayerArray. */
UCLASS()
class ECHOSTRIKE_API AEchoGameState : public AGameStateBase
{
	GENERATED_BODY()
};
