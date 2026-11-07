#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EchoGameMode.generated.h"

/** Server-only rules. The framework selects clear PlayerStarts and possesses each pawn. */
UCLASS()
class ECHOSTRIKE_API AEchoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEchoGameMode();
};
