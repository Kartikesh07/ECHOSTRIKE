#include "Core/EchoGameMode.h"
#include "Core/EchoGameState.h"
#include "Core/EchoPlayerState.h"
#include "Player/EchoCharacter.h"
#include "Player/EchoPlayerController.h"

AEchoGameMode::AEchoGameMode()
{
	DefaultPawnClass = AEchoCharacter::StaticClass();
	PlayerControllerClass = AEchoPlayerController::StaticClass();
	GameStateClass = AEchoGameState::StaticClass();
	PlayerStateClass = AEchoPlayerState::StaticClass();
}
