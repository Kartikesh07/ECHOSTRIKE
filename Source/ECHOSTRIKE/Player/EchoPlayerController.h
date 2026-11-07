#pragma once

#include "CoreMinimal.h"
#include "ECHOSTRIKEPlayerController.h"
#include "EchoPlayerController.generated.h"

/** Local cursor projection; aim yaw uses standard CharacterMovement networking. */
UCLASS()
class ECHOSTRIKE_API AEchoPlayerController : public AECHOSTRIKEPlayerController
{
	GENERATED_BODY()

public:
	AEchoPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void UpdateRotation(float DeltaTime) override;

	/** Fixed world-space Z, independent of obstacles, jumping and surface traces. */
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Aiming", meta = (Units = "cm"))
	float AimPlaneHeight = 0.0f;

	/** Retain the last aim when the plane intersection is too close to the pawn. */
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Aiming", meta = (ClampMin = "1.0", Units = "cm"))
	float AimDeadZone = 40.0f;

	/** Also reject the ambiguous cursor position directly over the character. */
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Aiming", meta = (ClampMin = "0.0"))
	float CursorDeadZonePixels = 12.0f;

private:
	bool GetCursorAimYaw(float& OutYaw) const;
};
