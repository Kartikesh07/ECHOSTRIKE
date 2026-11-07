#include "Core/EchoGameMode.h"
#include "Core/EchoGameState.h"
#include "Core/EchoPlayerState.h"
#include "Player/EchoCharacter.h"
#include "Player/EchoPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

AEchoGameMode::AEchoGameMode()
{
	DefaultPawnClass = AEchoCharacter::StaticClass();
	PlayerControllerClass = AEchoPlayerController::StaticClass();
	GameStateClass = AEchoGameState::StaticClass();
	PlayerStateClass = AEchoPlayerState::StaticClass();
}

void AEchoGameMode::RequestRespawn(AEchoCharacter* EliminatedCharacter)
{
	if (!HasAuthority() || !IsValid(EliminatedCharacter) || !EliminatedCharacter->IsEliminated())
	{
		return;
	}
	AController* Player = EliminatedCharacter->GetController();
	if (!IsValid(Player) || Player->GetPawn() != EliminatedCharacter || PendingRespawns.Contains(Player))
	{
		return;
	}
	PendingRespawns.Add(Player).Pawn = EliminatedCharacter;
	ScheduleRespawn(Player, RespawnDelay);
}

void AEchoGameMode::ScheduleRespawn(TWeakObjectPtr<AController> Player, float Delay)
{
	FPendingRespawn& Pending = PendingRespawns.FindChecked(Player);
	GetWorldTimerManager().SetTimer(Pending.Timer,
		FTimerDelegate::CreateUObject(this, &AEchoGameMode::CompleteRespawn, Player),
		FMath::IsFinite(Delay) ? FMath::Max(0.1f, Delay) : 3.0f, false);
}

void AEchoGameMode::CompleteRespawn(TWeakObjectPtr<AController> Player)
{
	FPendingRespawn* Pending = PendingRespawns.Find(Player);
	if (!Pending)
	{
		return;
	}
	AController* Controller = Player.Get();
	AEchoCharacter* OldPawn = Pending->Pawn.Get();
	// Never replace a pawn assigned by another gameplay flow while the timer was pending.
	if (!IsValid(Controller) || (Controller->GetPawn() && Controller->GetPawn() != OldPawn))
	{
		if (IsValid(OldPawn) && !OldPawn->GetController())
		{
			OldPawn->Destroy();
		}
		PendingRespawns.Remove(Player);
		return;
	}
	if (IsValid(OldPawn))
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}
	// Choose again instead of reusing the previous start, and never fall back to world origin.
	AActor* Start = ChoosePlayerStart(Controller);
	if (IsValid(Start) && Start->IsA<APlayerStart>())
	{
		RestartPlayerAtPlayerStart(Controller, Start);
	}
	if (Controller->GetPawn())
	{
		PendingRespawns.Remove(Player);
	}
	else
	{
		ScheduleRespawn(Player, RespawnRetryDelay);
	}
}

void AEchoGameMode::RestartPlayer(AController* NewPlayer)
{
	// Framework/client restart requests cannot bypass an elimination's delay.
	if (!PendingRespawns.Contains(NewPlayer))
	{
		Super::RestartPlayer(NewPlayer);
	}
}

void AEchoGameMode::Logout(AController* Exiting)
{
	if (FPendingRespawn* Pending = PendingRespawns.Find(Exiting))
	{
		GetWorldTimerManager().ClearTimer(Pending->Timer);
		if (AEchoCharacter* Pawn = Pending->Pawn.Get(); IsValid(Pawn) && Pawn->GetController() != Exiting)
		{
			Pawn->Destroy();
		}
		PendingRespawns.Remove(Exiting);
	}
	Super::Logout(Exiting);
}

void AEchoGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto& Entry : PendingRespawns)
	{
		GetWorldTimerManager().ClearTimer(Entry.Value.Timer);
	}
	PendingRespawns.Empty();
	Super::EndPlay(EndPlayReason);
}
