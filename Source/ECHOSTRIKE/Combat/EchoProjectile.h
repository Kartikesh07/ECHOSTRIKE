#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** Server-owned collision and lifetime; one replicated actor represents each shot. */
UCLASS()
class ECHOSTRIKE_API AEchoProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEchoProjectile();
	void InitializeFlight(float Speed, float Lifetime);
	float GetCollisionRadius() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage", meta = (ClampMin = "0.0"))
	float Damage = 20.0f;

	virtual void BeginPlay() override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	bool bImpactHandled = false;

	UFUNCTION()
	void OnImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
};
