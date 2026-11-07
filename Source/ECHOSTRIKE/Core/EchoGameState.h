#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "EchoGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FEchoRosterChanged);

/** Replicated global framework state, including the connected PlayerArray. */
UCLASS()
class ECHOSTRIKE_API AEchoGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	FEchoRosterChanged OnRosterChanged;
	virtual void AddPlayerState(APlayerState* Player) override;
	virtual void RemovePlayerState(APlayerState* Player) override;
};
