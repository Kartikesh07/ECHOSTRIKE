#pragma once

#include "CoreMinimal.h"
#include "ECHOSTRIKEPlayerController.h"
#include "EchoPlayerController.generated.h"

class UEchoHUDWidget;

/** Local cursor projection; aim yaw uses standard CharacterMovement networking. */
UCLASS()
class ECHOSTRIKE_API AEchoPlayerController : public AECHOSTRIKEPlayerController
{
	GENERATED_BODY()

public:
	AEchoPlayerController();
	virtual void SetPawn(APawn* InPawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetRespawnDeadline(double Deadline);
	double GetRespawnDeadline() const { return RespawnDeadline; }

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	UPROPERTY(EditDefaultsOnly, Category="HUD") TSubclassOf<UEchoHUDWidget> HUDWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="HUD") TSubclassOf<UUserWidget> CrosshairWidgetClass;
private:
	UPROPERTY(Transient) TObjectPtr<UEchoHUDWidget> HUDWidget;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> CrosshairWidget;
	UPROPERTY(ReplicatedUsing=OnRep_RespawnDeadline) double RespawnDeadline = -1;
	UFUNCTION() void OnRep_RespawnDeadline();
	bool GetCursorAimYaw(float& OutYaw) const;
};
