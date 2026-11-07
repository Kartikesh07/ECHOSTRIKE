#pragma once

#include "CoreMinimal.h"
#include "ECHOSTRIKECharacter.h"
#include "EchoCharacter.generated.h"

/** Template movement, camera and input with CharacterMovement networking. */
UCLASS()
class ECHOSTRIKE_API AEchoCharacter : public AECHOSTRIKECharacter
{
	GENERATED_BODY()

public:
	AEchoCharacter();
};
