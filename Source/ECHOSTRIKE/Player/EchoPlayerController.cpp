#include "Player/EchoPlayerController.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

AEchoPlayerController::AEchoPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContext(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseContext(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	DefaultMappingContexts.Add(DefaultContext.Object);
	MobileExcludedMappingContexts.Add(MouseContext.Object);
}
