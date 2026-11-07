#include "Core/EchoGameState.h"
void AEchoGameState::AddPlayerState(APlayerState* Player) { Super::AddPlayerState(Player); OnRosterChanged.Broadcast(); }
void AEchoGameState::RemovePlayerState(APlayerState* Player) { Super::RemovePlayerState(Player); OnRosterChanged.Broadcast(); }
