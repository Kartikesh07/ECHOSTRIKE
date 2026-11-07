#include "Core/EchoPlayerState.h"
#include "TimeEcho/EchoRecorderComponent.h"

AEchoPlayerState::AEchoPlayerState()
{
	EchoRecorder = CreateDefaultSubobject<UEchoRecorderComponent>(TEXT("EchoRecorder"));
}
