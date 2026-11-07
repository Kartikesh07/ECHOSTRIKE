#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEchoHealthChanged, UEchoHealthComponent*, HealthComponent, float, Health, float, PreviousHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEchoEliminated, UEchoHealthComponent*, HealthComponent);

/** Damage is consumed only on authority. Notifications run on the server and replicas. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ECHOSTRIKE_API UEchoHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEchoHealthComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsEliminated() const { return Health <= 0.0f; }

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FEchoHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FEchoEliminated OnEliminated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

private:
	bool bEliminationNotified = false;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.0f;

	UFUNCTION()
	void OnRep_Health(float PreviousHealth);
	UFUNCTION() void OnRep_MaxHealth();

	UFUNCTION()
	void HandleDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
};
