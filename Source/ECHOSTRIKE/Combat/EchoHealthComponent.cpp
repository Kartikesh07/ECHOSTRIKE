#include "Combat/EchoHealthComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UEchoHealthComponent::UEchoHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEchoHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(1.0f, MaxHealth) : 100.0f;
		const float PreviousHealth = Health;
		Health = MaxHealth;
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UEchoHealthComponent::HandleDamage);
		OnHealthChanged.Broadcast(this, Health, PreviousHealth);
	}
}

void UEchoHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetOwner()->OnTakeAnyDamage.RemoveDynamic(this, &UEchoHealthComponent::HandleDamage);
	Super::EndPlay(EndPlayReason);
}

void UEchoHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEchoHealthComponent, MaxHealth);
	DOREPLIFETIME(UEchoHealthComponent, Health);
}

void UEchoHealthComponent::HandleDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	if (!GetOwner()->HasAuthority() || IsEliminated() || !FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return;
	}
	const float PreviousHealth = Health;
	Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
	GetOwner()->ForceNetUpdate();
	OnRep_Health(PreviousHealth);
}

void UEchoHealthComponent::OnRep_Health(float PreviousHealth)
{
	const bool bNotifyElimination = !bEliminationNotified && IsEliminated();
	bEliminationNotified |= bNotifyElimination;
	OnHealthChanged.Broadcast(this, Health, PreviousHealth);
	if (bNotifyElimination)
	{
		OnEliminated.Broadcast(this);
	}
}

void UEchoHealthComponent::OnRep_MaxHealth()
{
	OnHealthChanged.Broadcast(this, Health, Health);
}
