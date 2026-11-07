#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EchoGameMode.generated.h"

class AEchoCharacter;

/** Server-only rules. The framework selects clear PlayerStarts and possesses each pawn. */
UCLASS()
class ECHOSTRIKE_API AEchoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEchoGameMode();
	void RequestRespawn(AEchoCharacter* EliminatedCharacter);
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0.1", Units = "s"))
	float RespawnDelay = 3.0f;

	/** Retry if every start is obstructed or pawn spawning temporarily fails. */
	UPROPERTY(EditDefaultsOnly, Category = "Respawn", meta = (ClampMin = "0.1", Units = "s"))
	float RespawnRetryDelay = 1.0f;

private:
	struct FPendingRespawn
	{
		FTimerHandle Timer;
		TWeakObjectPtr<AEchoCharacter> Pawn;
	};
	TMap<TWeakObjectPtr<AController>, FPendingRespawn> PendingRespawns;
	void CompleteRespawn(TWeakObjectPtr<AController> Player);
	void ScheduleRespawn(TWeakObjectPtr<AController> Player, float Delay);
};
