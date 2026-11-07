#pragma once

#include "CoreMinimal.h"
#include "ECHOSTRIKECharacter.h"
#include "EchoCharacter.generated.h"

/** Fixed arena camera and movement independent of the controller's aim yaw. */
UCLASS()
class ECHOSTRIKE_API AEchoCharacter : public AECHOSTRIKECharacter
{
	GENERATED_BODY()

public:
	AEchoCharacter();

	virtual void DoMove(float Right, float Forward) override;
	virtual void DoLook(float Yaw, float Pitch) override;
};
