#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EchoCharacterMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FEchoDashHUDChanged, float, float, bool);

/** Only corrections carry dash simulation state; ordinary move acknowledgements stay small. */
struct FEchoMoveResponse : FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;
	float CooldownRemaining = 0.0f;
	float DashRemaining = 0.0f;
	FVector_NetQuantizeNormal DashDirection = FVector::ForwardVector;
	FVector_NetQuantizeNormal LastDirection = FVector::ForwardVector;
	virtual void ServerFillResponseData(const UCharacterMovementComponent& Movement, const FClientAdjustment& Adjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& Movement, FArchive& Ar, UPackageMap* Map) override;
};

/** Predicted dash intent uses one compressed move bit, never a separate dash RPC. */
UCLASS()
class ECHOSTRIKE_API UEchoCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UEchoCharacterMovementComponent();
	void RequestDash();
	FEchoDashHUDChanged OnDashHUDChanged;

	UFUNCTION(BlueprintPure, Category = "Dash")
	bool IsDashing() const { return MovementMode == MOVE_Custom && CustomMovementMode == DashMovementMode; }

	UFUNCTION(BlueprintPure, Category = "Dash")
	float GetDashCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Dash")
	FVector GetDashDirection() const { return DashDirection; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "1.0", Units = "cm"))
	float DashDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", Units = "s"))
	float DashDuration = 0.18f;

	/** Start-to-start cooldown, measured in validated movement simulation time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", Units = "s"))
	float DashCooldown = 2.0f;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& Response) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMode, uint8 PreviousCustomMode) override;
	virtual void ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase,
		FName ClientBaseBoneName, uint8 ClientMovementMode) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FEchoDashTest;
#endif
	friend class FEchoSavedMove;
	friend struct FEchoMoveResponse;
	static constexpr uint8 DashMovementMode = 1;
	bool CanStartDash() const;
	void FinishDash();
	bool bDashRequested = false;
	float CooldownRemaining = 0.0f;
	float DashRemaining = 0.0f;
	FVector LastDirection = FVector::ForwardVector;
	FVector DashDirection = FVector::ForwardVector;
	FEchoMoveResponse MoveResponse;
	FEchoMoveResponse PendingDashState;
	float PendingDashTimeStamp = -1.0f;
};
