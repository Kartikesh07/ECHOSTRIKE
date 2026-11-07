#pragma once

#include "CoreMinimal.h"
#include "ECHOSTRIKECharacter.h"
#include "EchoCharacter.generated.h"

class AEchoProjectile;
class UInputAction;
class UEchoHealthComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FEchoShotFired, const FTransform&, float, float);

/** Fixed arena camera and movement independent of the controller's aim yaw. */
UCLASS()
class ECHOSTRIKE_API AEchoCharacter : public AECHOSTRIKECharacter
{
	GENERATED_BODY()

public:
	AEchoCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	/** Raised only after a projectile has successfully finished spawning on authority. */
	FEchoShotFired OnShotFired;
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsEliminated() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UEchoHealthComponent> HealthComponent;

	virtual void DoMove(float Right, float Forward) override;
	virtual void DoLook(float Yaw, float Pitch) override;

protected:
	virtual void PostInitializeComponents() override;
	virtual bool CanJumpInternal_Implementation() const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Health", meta = (DisplayName = "On Eliminated"))
	void OnEliminated();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Input")
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<AEchoProjectile> ProjectileClass;

	/** Shots per second, enforced by the server across presses and releases. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float FireRate = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "1.0", Units = "cm/s"))
	float ProjectileSpeed = 2400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.1", Units = "s"))
	float ProjectileLifetime = 3.0f;

	/** Character-local offset, rotated by the server's current character facing. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (Units = "cm"))
	FVector ProjectileSpawnOffset = FVector(70.0, 0.0, 0.0);

private:
	UFUNCTION()
	void HandleEliminated(UEchoHealthComponent* Component);

	void StartFiring();
	void RequestDash();
	void StopFiring();
	void FireShot();
	float GetFireInterval() const;

	UFUNCTION(Server, Reliable)
	void ServerSetFiring(bool bFiring);

	FTimerHandle FireTimer;
	double NextServerShotTime = 0.0;
};
