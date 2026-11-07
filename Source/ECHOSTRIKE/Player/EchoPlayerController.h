#pragma once

#include "CoreMinimal.h"
#include "ECHOSTRIKEPlayerController.h"
#include "EchoPlayerController.generated.h"

/** Inherits local-player-only Enhanced Input setup and framework camera ownership. */
UCLASS()
class ECHOSTRIKE_API AEchoPlayerController : public AECHOSTRIKEPlayerController
{
	GENERATED_BODY()

public:
	AEchoPlayerController();
};
